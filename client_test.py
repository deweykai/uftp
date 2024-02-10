#!/usr/bin/env python3

import subprocess
import select
import time
import os
import logging

logging.basicConfig(format='%(asctime)s [%(levelname)s] %(message)s', level=logging.DEBUG)

def run_ftp_command(command):
    current_dir = os.path.dirname(os.path.abspath(__file__))
    test_clientdir = os.path.join(current_dir, 'test_clientdir')
    process = subprocess.Popen(['../ftp_client', 'zeus', '1234'], cwd=test_clientdir, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    output, err = process.communicate(command + '\n', timeout=5)
    process.kill()
    logging.debug(output.strip())
    if len(err.strip()) > 0:
        logging.error(err.strip())

def run_ftp_client(commands):
    for command in commands:
        logging.info(f'Running : {command}')
        run_ftp_command(command)

commands = [
    'put photo.png',
    'get photo.png',
    'delete photo.png',
    'ls',
    'exit',
]

run_ftp_client(commands)
# run_ftp_command('put photo.png')
# run_ftp_command('put movie.mov')