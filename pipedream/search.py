#!/usr/bin/env python

from hello_m0ther import api_get
from environment import get_setting

def slowsearch():
    return api_get("/api/service",
            {"identity":get_setting("identity")})