import cgi
import os
import urllib
import logging

from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from google.appengine.ext import db
from google.appengine.ext.webapp import template
from google.appengine.api import memcache

from Identity import Identity, get_identity

# Set the debug level
_DEBUG = True


def get_greeting_user(identity):
  return GreetingUser.all().filter("greeting_user =",identity).get()
def intersperse(greetings,greetingsuser):
  result = []
  coreyross = Greeting.all().filter("author =",None).filter("super_secret_coreyross_for =",greetingsuser).order('date').fetch(limit=5)
  logging.info("coreyross %d" % len(coreyross))
  for i in range(0,len(greetings)):
    if len(coreyross) > 0:
      if coreyross[0].date < greetings[i].date:
        result.append(coreyross[0])
        del coreyross[0]
    result.append(greetings[i])
  return result + coreyross
    
    
class GreetingUser(db.Model):
  greeting_user = db.ReferenceProperty(reference_class=Identity)
  joined = db.DateTimeProperty(auto_now_add=True)
  nickname = db.StringProperty(required=True)
  
class Greeting(db.Model):
  author = db.ReferenceProperty(reference_class=GreetingUser,collection_name="author")
  content = db.StringProperty(multiline=True)
  date = db.DateTimeProperty(auto_now_add=True)
  room = db.StringProperty()
  super_secret_coreyross_for = db.ReferenceProperty(reference_class=GreetingUser,collection_name="coreyross")

class BaseRequestHandler(webapp.RequestHandler):
  """Base request handler extends webapp.Request handler

     It defines the generate method, which renders a Django template
     in response to a web request
  """

  def generate(self, template_name, template_values={}):
    """Generate takes renders and HTML template along with values
       passed to that template

       Args:
         template_name: A string that represents the name of the HTML template
         template_values: A dictionary that associates objects with a string
           assigned to that object to call in the HTML template.  The defualt
           is an empty dictionary.
    """
    # We check if there is a current user and generate a login or logout URL
    user = users.get_current_user()

    if user:
      log_in_out_url = users.create_logout_url('/')
    else:
      log_in_out_url = users.create_login_url(self.request.path)

    # We'll display the user name if available and the URL on all pages
    values = {'user': user, 'log_in_out_url': log_in_out_url}
    values.update(template_values)

    # Construct the path to the template
    directory = os.path.dirname(__file__)
    path = os.path.join(directory, 'templates', template_name)

    # Respond to the request by rendering the template
    self.response.out.write(template.render(path, values, debug=_DEBUG))
    
class MainRequestHandler(BaseRequestHandler):
  def get(self):
 

    self.generate('index.html', {"identity":self.request.get("identity"),"room":self.request.get("room")});

class ChatsRequestHandler(BaseRequestHandler):
  def renderChats(self,greetinguser,room=None):
    if room != None and room.startswith("pm"):
      userList=room[2:]
      users = userList.split("$")
      logging.info(users)
      allowed = False
      for user in users:
        if greetinguser.nickname==user:
          allowed = True
          break
      if not allowed: raise Exception("You're not allowed.")
    
    greetings_query = Greeting.all().order('date').filter("room =",room).filter("super_secret_coreyross_for =",None)
    greetings = greetings_query.fetch(1000)
    template_values = {
      'greetings': greetings,
    }
    return greetings
      
  def getChats(self, greetinguser,room=None,cache=True):
    if cache:
      greetings = memcache.get("chat"+str(room))
    else: greetings = None
    if greetings is None:
      logging.info("gotta query db chat"+str(room))
      greetings = self.renderChats(greetinguser,room)
      if greetings==None: greetings = []
      logging.info("greetings are %s" % greetings)
      if not memcache.set("chat"+str(room), greetings, 10):
        logging.error("Memcache set failed:")
    if room==None:
        greetings = intersperse(greetings,greetinguser)
    return self.generate('chats.html',{'greetings':greetings,'greetinguser':greetinguser})
    
  def get(self):
    identity = get_identity(self)
    greetinguser= get_greeting_user(identity)
    room = self.request.get("room")
    if room=="": room=None
    self.getChats(greetinguser,room)

  def post(self):
    identity = get_identity(self)
    greetinguser = get_greeting_user(identity)
    room = self.request.get("room")
    if room=="": room=None
    greeting = Greeting()
    greeting.author = greetinguser
    greeting.content = self.request.get('content')
    greeting.room = room
    greeting.put()
    if room != None and room.startswith("pm"):
      users = room.split("$")
      for user in users[1:]:
        g = Greeting()
        g.author = None
        g.content = "You've got a new PM from %s" % users[0][2:]
        g.room = None
        g.super_secret_coreyross_for = greetinguser
        g.put()
    self.getChats(greetinguser,room,cache=False)

    
class EditUserProfileHandler(BaseRequestHandler):
  """This allows a user to edit his or her wiki profile.  The user can upload
     a picture and set a feed URL for personal data
  """
  def get(self, user):
    # Get the user information
    unescaped_user = urllib.unquote(user)
    greeting_user_object = users.User(unescaped_user)
    # Only that user can edit his or her profile
    if users.get_current_user() != greeting_user_object:
      self.redirect('/view/StartPage')

    greeting_user = GreetingUser.gql('WHERE greeting_user = :1', greeting_user_object).get()
    if not greeting_user:
      greeting_user = GreetingUser(greeting_user=greeting_user_object)
      greeting_user.put()

    self.generate('edit_user.html', template_values={'queried_user': greeting_user})

  def post(self, user):
    # Get the user information
    unescaped_user = urllib.unquote(user)
    greeting_user_object = users.User(unescaped_user)
    # Only that user can edit his or her profile
    if users.get_current_user() != greeting_user_object:
      self.redirect('/')

    greeting_user = GreetingUser.gql('WHERE greeting_user = :1', greeting_user_object).get()

    greeting_user.picture = self.request.get('user_picture')
    greeting_user.website = self.request.get('user_website')
    greeting_user.seated = self.request.get('user_seated')
    greeting_user.put()


    self.redirect('/user/%s' % user)
    
class UserProfileHandler(BaseRequestHandler):
  """Allows a user to view another user's profile.  All users are able to
     view this information by requesting http://wikiapp.appspot.com/user/*
  """

  def get(self, user):
    """When requesting the URL, we find out that user's WikiUser information.
       We also retrieve articles written by the user
    """
    # Webob over quotes the request URI, so we have to unquote twice
    unescaped_user = urllib.unquote(urllib.unquote(user))

    # Query for the user information
    greeting_user_object = users.User(unescaped_user)
    greeting_user = GreetingUser.gql('WHERE greeting_user = :1', greeting_user_object).get()

    # Generate the user profile
    self.generate('user.html', template_values={'queried_user': greeting_user})
class NickNameHandler(webapp.RequestHandler):
  def get(self):
    identity = get_identity(self)
    if get_greeting_user(identity)==None:
      self.response.out.write("NOTSET")
    else:
      self.response.out.write("SET")
  def post(self):
    identity = get_identity(self)
    if get_greeting_user(identity)!=None:
      raise Exception("nickname already set for this identity")
    if GreetingUser.all().filter("nickname =",self.request.get("nickname")).get()!=None:
      raise Exception("nickname already taken; nice try")
    g = GreetingUser(greeting_user=identity,nickname=self.request.get("nickname"))
    g.put()
    

                                                
application = webapp.WSGIApplication(
                                     [('/chat', MainRequestHandler),
                                      ('/chat/',MainRequestHandler),
                                      ('/chat/getchats', ChatsRequestHandler),
                                      ('/chat/user/([^/]+)', UserProfileHandler),
                                      ('/chat/edituser/([^/]+)', EditUserProfileHandler),
                                      ('/chat/nickname',NickNameHandler)],
                                     debug=True)

def main():
  run_wsgi_app(application)

if __name__ == "__main__":
  main()