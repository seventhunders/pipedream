#!/usr/bin/env python

from google.appengine.ext import db

def make_identity(user):
    chars = "abcdefghijklmnopqrstuvwxyz123456789"
    rnd = ""
    from random import choice
    for i in range(0,32):
        rnd += choice(chars)
    user.identity = rnd
def get_identity(handler):
        id = Identity.all().filter("identity =",handler.request.get("identity")).get()
        if id is None:
            raise Exception("No ID for you...")
        return id

def get_permission(handler):
    permission = Permission.get(handler.request.get("permission"))
    if permission==None or not is_permission_still_valid(permission):
        raise Exception("No permission for you...")
    return permission
def make_permissions(user):
    if user.friend_permission is None or not is_permission_still_valid(user.friend_permission):
        p = Permission(bestowing_user=user)
        p.put()
        user.friend_permission = p
        user.put()
    if user.frogger_permission is None or not is_permission_still_valid(user.frogger_permission):
        p = Permission(bestowing_user=user,frogger=True)
        p.put()
        user.frogger_permission = p
        user.put()

def get_level_one_permissions_for_user(u):
    result = []
    for they_trust_me in u.these_people_trust_me:
        make_permissions(they_trust_me)
        result.append(they_trust_me.friend_permission)
        result.append(they_trust_me.frogger_permission)
    return result
def get_more_permissions(p):
    result = []
    id = p.bestowing_user
    for next_leveL_trusted in id.they_trust_me:
        make_permissions(next_level_trusted)
        result.append(next_level_trusted.frogger_permission)
    return result
    
def is_permission_still_valid(p):
    if p.created > (datetime.now() - timedelta(days=1)):
        return False
    return True

class Identity(db.Model):
    name = db.StringProperty()
    these_people_trust_me = db.ListProperty(item_type=db.Key)
    identity = db.StringProperty()
    created_date = db.DateTimeProperty(auto_now_add=True)
    accessed_date = db.DateTimeProperty(auto_now=True)
    friendPermission = db.ReferenceProperty()
    froggerPermission = db.ReferenceProperty(collection_name="froggerpermission")
    
class Permission(db.Model):
    created = db.DateTimeProperty(auto_now_add=True)
    bestowing_user = db.ReferenceProperty(Identity)
    frogger = db.BooleanProperty(default=False)