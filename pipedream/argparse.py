#!/usr/bin/env python

def parse(string):
    quoted = False
    buffer = ""
    result = []
    for letter in string:
        if letter==" " and not quoted:
            result.append(buffer)
            buffer = ""
        elif letter=='"':
            quoted = not quoted
        else:
            buffer += letter
    result.append(buffer)
    return result