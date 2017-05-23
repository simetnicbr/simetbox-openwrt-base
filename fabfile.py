from __future__ import with_statement
from fabric.api import settings, abort, roles, get, put, open_shell
from fabric.api import local, run, env, cd, runs_once
from fabric.contrib.console import confirm
from fabric.contrib.project import rsync_project

LOCAL_DISTDIR='simet-client-1/'
PACKAGE_STRING='package/simet-client/'
PACKAGE_IPK_FILE='bin/ar71xx/packages/simet-client_1_ar71xx.ipk'
env.builddir = '/home/rafael/openwrt_sdk/'


env.hosts = ['rafael@200.160.6.16', 'root@200.160.6.4']
env.roledefs = {'builders'  : ['200.160.6.16'],
                'wrt' : ['root@200.160.6.4']}


@runs_once
def update_src():
    print env.host
    local('make distdir')
    rsync_project(env.builddir + PACKAGE_STRING + 'src', LOCAL_DISTDIR)
    local('rm -rf '+ PACKAGE_STRING)


@roles('builders')
def compile():
    with cd(env.builddir):
        run('make -j 8 '+PACKAGE_STRING+'clean V=99')
        run('make V=99')


@roles('builders')
def get_ipk():
    build()
    get(env.builddir + PACKAGE_IPK_FILE, '.')

@roles('builders')
def build():
    update_src()
    local('rm -rf simet-client-1/')
    compile()

@roles('wrt')
def send_ipk():
    local('scp simet-client_1_ar71xx.ipk ' + env.user+'@' + env.host + ':')
    local('rm -rf simet-client_1_ar71xx.ipk')


@roles('wrt')
def deploy():
    local('fab get_ipk')
    run('rm -rf simet-client data.tar.gz')
    send_ipk()
    run('tar -xzvf simet-client_1_ar71xx.ipk')
    run('tar -xzvf data.tar.gz')
    run('cp $HOME/usr/bin/simet_client .')


@roles('wrt')
def test():
    deploy()
    run('/root/simet_client -t tcp')
