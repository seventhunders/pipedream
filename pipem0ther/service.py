#!/usr/bin/env python

from google.appengine.ext import db
from Identity import Identity, Permission, make_identity, is_permission_still_valid, make_permissions

from datetime import datetime, timedelta

VISIBILITY_NONE = 0
VISIBILITY_FRIENDS = 1
VISIBILITY_FROGGER = 2
VISIBILITY_PUBLIC = 3


def safe_props(svc):
    result = []
    for s in svc:
        result.append({"key":str(s.key()),
                       "shortname":s.shortname,
                       "protocol":s.protocol,
                       "description":s.description
                       
        })
    return result
def get_service(handler,owner):
    a = Service.get(handler.request.get("service"))
    if a.owner.key()!= owner.key():
        raise Exception("You can't have this service")
    return a

def find_svcs_for_permission(p):
    if not permission_still_valid(p): return []
    id = p.bestowing_user
    s = Service.all()
    s = s.filter("owner =",id).filter("online =",True)
    result = []
    if not p.frogger:
        result.append(s.filter("visibility =",VISIBILITY_FRIENDS).fetch(limit=1000))
    result.append(s.filter("visibility =",VISIBILITY_FROGGER).fetch(limit=1000))
def can_connect_to_svc_with(p, svc):
    id = svc.owner
    make_permissions(id)
    if not is_permission_still_valid:
        raise Exception("Invalid permission")
    if svc.visibility==VISIBILITY_FRIENDS:
        if id.friend_permission.key()==p.key():
            return True
        else:
            raise Exception("Friend check failed.")
    elif svc.visibility==VISIBILITY_FROGGER:
        if id.friend_permission.key()==p.key():
            return True
        elif id.frogger_permission.key()==p.key():
            return True
        else:
            raise Exception("Frogger check failed")
    else:
        raise Exception("Can't check that visibility here")
    raise Exception("WTF")

def dont_be_stupid_connect_to_service(s,user,connecting_from_uri):
    o = OneTimePad(requesting_user = user,for_svc = s,connecting_from_uri=connecting_from_uri)
    from random import choice
    choices = "abcdefghijklmnopqrstuvwxyz1234567890"
    one_time_pad=""
    for i in range(0,32):
        one_time_pad += choice(choices)
    o.one_time_pad = one_time_pad
    o.put()
    return o



        
    


class Service(db.Model):
    shortname = db.StringProperty(required=True)
    protocol = db.StringProperty(required=True)
    owner = db.ReferenceProperty(reference_class=Identity)
    description = db.StringProperty()
    visibility = db.IntegerProperty(default=VISIBILITY_NONE)
    online = db.BooleanProperty(default=True)
    oneline_as_of = db.DateTimeProperty(auto_now_add=True)
    official_uri=db.StringProperty()
    
class OneTimePad(db.Model):
    created = db.DateTimeProperty(auto_now_add=True)
    requesting_user = db.ReferenceProperty(reference_class=Identity)
    one_time_pad = db.StringProperty()
    connecting_from_uri = db.StringProperty()
    for_svc = db.ReferenceProperty(reference_class=Service)
    