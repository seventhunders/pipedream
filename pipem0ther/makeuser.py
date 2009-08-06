#!/usr/bin/env python

import wsgiref.handlers


from google.appengine.ext import webapp
from Identity import Identity, make_identity

class MainHandler(webapp.RequestHandler):

  def get(self):
    self.response.out.write("""
                            <form method="post">
                            Pick a name <br />
                            <input name="name" type="text" />
                            <input type="submit" />
                            </form>
                            
                            """)
  def post(self):
       self.response.headers["Content-Type"]="application/octet-stream; name=identity.file"
       self.response.headers["Content-disposition"] = "attachment; filename=\"identity.file\""
       u = Identity(name=self.request.get("name"))
       make_identity(u)
       u.put()
       self.response.out.write(u.identity)

def main():
  application = webapp.WSGIApplication([('/admin/makeuser', MainHandler)],
                                       debug=True)
  wsgiref.handlers.CGIHandler().run(application)


if __name__ == '__main__':
  main()
