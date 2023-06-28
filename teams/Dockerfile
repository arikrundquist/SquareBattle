#syntax=docker/dockerfile:1

FROM fedora:latest
RUN dnf install g++ -y

WORKDIR /root/teams/team

COPY ./api/ /root/api/

CMD timeout 30 make team.so
