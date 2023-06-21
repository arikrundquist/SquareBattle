#!/usr/bin/python

import mmap
from PIL import Image
import numpy as np
import os
import pygame
from time import sleep
import ctypes

class Header(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ('frame_ticker', ctypes.c_bool),
        ('buffer_size', ctypes.c_uint32),
    ]

offset = ctypes.sizeof(Header)
with open("game.graphics", "r+") as f:
    header_buff = mmap.mmap(f.fileno(), offset)
header = Header.from_buffer(header_buff)
mm = np.memmap("game.graphics", offset=offset, shape=(header.buffer_size, header.buffer_size, 3))

pygame.init()
size = 0
initial_size = header.buffer_size
while initial_size < 512:
    initial_size *= 2
screen = pygame.display.set_mode((initial_size, initial_size), pygame.RESIZABLE)
while True:
    sleep(0.005)
    header = Header.from_buffer(header_buff)
    if pygame.QUIT in [event.type for event in pygame.event.get()]:
        break
    if header.frame_ticker:
        continue
    s = pygame.display.get_window_size()
    if min(s) != size:
        s = min(s)
    else:
        s = max(s)
    if s != size:
        size = s
        pygame.display.set_mode((size, size), pygame.RESIZABLE)
    img = pygame.surfarray.make_surface(mm)
    img = pygame.transform.scale(img, (size, size))
    screen.blit(img, (0, 0))
    pygame.display.update()
    header.frame_ticker = True
pygame.quit()
