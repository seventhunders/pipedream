import wsgiref.handlers


from google.appengine.ext import webapp
from Identity import Identity

class MainHandler(webapp.RequestHandler):

  def get(self):
    id = self.request.get("identity")
    i = Identity.all().filter("identity =",id).get()
    if i is None:
        self.response.out.write("NO")
        return
    self.response.out.write("YES")
    
class EchoHandler(webapp.RequestHandler):

  def get(self):
    self.response.out.write("YES")


def main():
  application = webapp.WSGIApplication([('/api/identity', MainHandler),
                                        ('/api/areyouthere',EchoHandler)],
                                       debug=True)
  wsgiref.handlers.CGIHandler().run(application)


if __name__ == '__main__':
  main()
#!/usr/bin/env python

