#syntax=docker/dockerfile:1

# i like fedora a lot, so that's what we're gonna use
FROM fedora:latest

# need g++ and python, plus xeyes for debugging
RUN dnf install g++ python xeyes which -y

# python dependencies
RUN python -m ensurepip --upgrade
RUN python -m pip install --upgrade pip
RUN python -m pip install numpy pygame Pillow

# copy everything over
WORKDIR /root/
COPY . .

# for now, just call make run to build and run the project
CMD make -f Makefile.docker run_docker
# use this to verify your setup
#CMD xeyes
