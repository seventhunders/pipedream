#!/usr/bin/env python

import wsgiref.handlers


from google.appengine.ext import webapp
from Identity import Identity, get_identity, get_permission, get_more_permissions, get_level_one_permissions_for_user
from service import Service, VISIBILITY_NONE, VISIBILITY_FRIENDS, VISIBILITY_FROGGER, VISIBILITY_PUBLIC
from service import get_service, find_svcs_for_permission, dont_be_stupid_connect_to_service, can_connect_to_svc_with, OneTimePad, safe_props
from datetime import datetime
import logging

class moreSvcHandler(webapp.RequestHandler):
    #for making a new service
  def post(self):
    id = get_identity(self)
    if Service.all().filter("shortname =",self.request.get("shortname")).get() != None:
        raise Exception("needs unique shortname")
    s = Service(shortname=self.request.get("shortname"),
                protocol = self.request.get("protocol"),
                description = self.request.get("description"),
                visibility = VISIBILITY_NONE,
                online = False,owner=id)
    s.put()
    self.response.out.write(s.key())
    
class MainHandler(webapp.RequestHandler):

  def get(self): #get services, optionally with some permission
    id = get_identity(self)
    permission = self.request.get("permission")
    #give them their own services
    result = Service.all().filter("owner =",id).fetch(limit=1000)
    if permission!="":
        p = get_permission(self)
        result.append(find_svcs_for_permission(p))
    #give them public services
    public = Service.all().filter("visibility =",VISIBILITY_PUBLIC).fetch(limit=1000)
    result += public
    #no duplicates
    r = {}
    for re in result:
        if not r.has_key(re.key()):
            r[re.key()]=re
    result = []
    for re in r:
        result.append(r[re])
    self.response.out.write(safe_props(result))
    
  def options(self): #get more permissions
    id = get_identity(self)
    permission = get_permission(self)
    self.response.out.write(get_more_permissions(permission))

  def head(self): #get permissions for user
    id = get_identity(self)
    self.response.out.write(get_level_one_permissions_for_user(id))
        

#for updating an existing service
  def post(self):
    id = get_identity(self)
    logging.info(self.request.get("service"))
    service = get_service(self,id)
    vis = self.request.get("visibility")
    if vis is not "":
        if vis=="NONE":
            service.visibility = VISIBILITY_NONE
        elif vis=="FRIENDS":
            service.visibility = VISIBILITY_FRIENDS
        elif vis=="FROGGER":
            service.visibility = VISIBILITY_FROGGER
        elif vis=="PUBLIC":
            service.visibility = VISIBILITY_PUBLIC
        else:
            raise Exception("Bad visibility: %s" % vis)
    online = self.request.get("online")
    if online=="YES":
        service.online = True
        service.official_uri = self.request.get("official_uri")
        service.online_as_of = datetime.now()
    elif online=="NO":
        service.online = False
    service.put()
    logging.info("completed OK")
    
  
  
  def delete(self):
    id = get_identity(self)
    service = get_service(self,id)
    service.delete()
            
    
class ConnectHandler(webapp.RequestHandler):
    def post(self):
        id = get_identity(self)
        connecting_from_uri = self.request.get("connecting_from_uri")
        if connecting_from_uri=="":
            raise Exception("No connecting from uri!  That's going to be fail")
        svc = Service.get(self.request.get("service"))
        if self.request.get("permission")=="":
            #maybe it's a public service?
            if svc.visibility!=VISIBILITY_PUBLIC:
                #maybe you own the service?
                svc = get_service(self,id)
            
            o = dont_be_stupid_connect_to_service(svc,id,connecting_from_uri)
            self.response.out.write(str(o.key()) + "\n")
            self.response.out.write(o.one_time_pad + "\n")
            self.response.out.write(svc.official_uri)
        else: #we have a permission
            p = get_permission(self)
            if not can_connect_to_svc_with(p,svc):
                raise Exception("WTF")
            o = dont_be_stupid_connect_to_service(svc,id,connecting_from_uri)
            self.response.out.write(str(o.key()) + "\n")
            self.response.out.write(o.one_time_pad + "\n")
            #self.response.out.write(svc.official_uri) #we should really let the frogger protocol figure this out
    def get(self): #handles the shouldVerifyPerson (computersaysno)
        id = get_identity(self)
        svc = get_service(self,id)
        logging.info(self.request.get("connecting_uri"))
        logging.info(str(svc.key()))
        otp = OneTimePad.all().filter("connecting_from_uri =",self.request.get("connecting_uri")).filter("for_svc =",svc).get()
        if otp != None:
            self.response.out.write("OK")
        else:
            self.response.out.write("NO")
            logging.info("Computer says no")
class OTPHandler(webapp.RequestHandler):
    def get(self):
        id = get_identity(self)
        svc = get_service(self,id)
        otp = OneTimePad.get(self.request.get("otp"))
        self.response.out.write(otp.one_time_pad)
        otp.delete()
            
            
 


def main():
  application = webapp.WSGIApplication([('/api/service', MainHandler),
                                        ('/api/moresvc',moreSvcHandler),
                                        ('/api/connect',ConnectHandler),
                                        ('/api/otp',OTPHandler)],
                                       debug=True)
  wsgiref.handlers.CGIHandler().run(application)


if __name__ == '__main__':
  main()
