# ftp_project

Basic self-made C-based ftp server. <br>
This is a course project of *Internet Applications*. The project is focusing on finishing
the requirement, so only several core functions are covered.

## Features:

* Limited user login (currently 2 users writern in code)
* File system command such as cd, cdup, pwd, mkdir, dele, rename....
* Upload and download in active or passive mode
* Ascii mode and binary mode transmission
* Display user's username, IP, actions, speed, total traffic...
* Limit download speed and upload speed
* Admin user can upload, mkdir, dele, rename while common users cannot

## How to Use This Project

This project is based on linux socket programming. There is no need to install any moudule before 
running, just compile and run by using following code:<br>
> gcc -o ftp_server ftp_server.c <br>
> sudo ./ftp_server <*speed_limit*> <br>
Speed limit can be any positive integer that will limit both upload and download speed to 
*speed_limit* kb/s.

## Can I modify/fix the project?

Of course! Any commit are welcomed, you can always submit a pull request! <br>
We welcome you to add new functions and modify existing functions.
