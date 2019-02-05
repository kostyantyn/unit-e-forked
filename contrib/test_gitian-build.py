# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/MIT.
#
# Run tests with `pytest`, requires `pytest-mock` to be installed.

from unittest.mock import call
import os

gitian_build = __import__('gitian-build')

class Log:
    def __init__(self, name):
        self.name = name
        self.create()

    def filename(self):
        return "test_data/gitian_build/" + self.name + ".log"

    def create(self):
        with open(self.filename(), "w") as file:
            file.write("Test: %s\n" % self.name)

    def log(self, line):
        with open(self.filename(), "a") as file:
            file.write(line + "\n")

    def check(self):
        with open(self.filename()) as file:
            with open(self.filename() + ".expected") as file_expected:
                assert file.read() == file_expected.read()

    def log_call(self, parameters, shell=None, stdout=None, stderr=None, universal_newlines=None, encoding=None):
        if isinstance(parameters, list):
            line = " ".join(parameters)
        else:
            line = parameters
        if shell:
            line += "  shell=" + str(shell)
        if stdout:
            line += "  stdout=" + str(stdout)
        if stderr:
            line += "  stderr=" + str(stdout)
        if universal_newlines:
            line += "  universal_newlines=" + str(universal_newlines)
        if encoding:
            line += "  encoding=" + str(encoding)
        self.log(line)
        if parameters[0:2] == ["git", "show"]:
            return "somecommit"
        else:
            return 0

    def log_chdir(self, parameter):
        self.log("chdir('%s')" % parameter)

def create_args(mocker):
    args = mocker.Mock()
    args.version = "someversion"
    args.jobs = "2"
    args.memory = "3000"
    args.commit = "v1.1.1"
    args.url = "/some/repo"
    args.git_dir = "unit-e"
    args.sign_prog = "gpg"
    args.signer = "somesigner"
    return args

def test_build(mocker):
    log = Log("test_build")

    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("os.makedirs")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("os.getcwd", return_value="somedir")

    gitian_build.build(create_args(mocker), "someworkdir")

    log.check()

def test_sign(mocker):
    log = Log("test_sign")

    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    args = create_args(mocker)

    gitian_build.sign(args, "someworkdir")

    log.check()

def test_verify(mocker):
    log = Log("test_verify")

    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    gitian_build.verify(create_args(mocker), "someworkdir")

    log.check()

def test_setup_linux(mocker):
    log = Log("test_setup_linux")

    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)

    gitian_build.setup(create_args(mocker), "someworkdir")

    log.check()

def test_prepare_git_dir(mocker):
    log = Log("test_prepare_git_dir")

    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.check_output", side_effect=log.log_call)

    gitian_build.prepare_git_dir(create_args(mocker), "someworkdir")

    log.check()