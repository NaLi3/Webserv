#!/usr/bin/env python3

import requests
import json

print("This is a very basic python script wherein we will first create a file called pikachu lol, then access its contents, and proceed to delete it.\n")

url = "http://localhost:8080/site/pikachulol"
myobj = {"pikachu": "better_than_raichu"}

x = requests.post(url, json = myobj)

print("Upon creating the file, we will receive code 201 if it is created or 200 if it already exists and is just modified\n")

print(x)

x = requests.get("http://localhost:8080/uploads/pikachulol")

print("Here we do a get request to access the contents of the pikachulol file\n")

print(x.text)
print(x)

print("\nAnd we will now delete pikachulol, thus receiving a 204 resource has been deleted answer")

x = requests.delete("http://localhost:8080/uploads/pikachulol")
print(x)