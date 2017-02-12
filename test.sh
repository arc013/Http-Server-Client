#!/bin/bash
PATH0="http://localhost:8000/index.html"
curl -H "Connection: close" $PATH0 &
curl -H "Connection: close" $PATH0 &
curl -H "Connection: close" $PATH0 &
curl -H "Connection: close" $PATH0 &
curl -H "Connection: close" $PATH0 &

PATH1="http://localhost:8000/subdir1/kitten.png"

PATH2="http://localhost:8000/subdir1/guinea.jpg"

PATH3="http://localhost:8000/subdir1/subdir2/index.html"

PATH3="http://localhost:8000/subdir1/subdir3/noaccess.html"



