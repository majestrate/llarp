#!/usr/bin/env bash
#
# create signed release tarball with submodules bundled
# usage: ./contrib/tarball.sh [keyid]
#
repo=$(readlink -e $(dirname $0)/..)
if [ ! -e $repo/.venv/bin/git-archive-all ] ; then
    python3 -m venv $repo/.venv && $repo/.venv/bin/pip3 install git_archive_all
fi
branch=$(test -e $repo/.git/ && git rev-parse --abbrev-ref HEAD)
out="lokinet-$(git describe --exact-match --tags $(git log -n1 --pretty='%h') 2> /dev/null || ( echo -n $branch- && git rev-parse --short HEAD)).tar.xz"
$repo/.venv/bin/git-archive-all -C $repo --force-submodules $out && rm -f $out.sig && (gpg -u ${1:-jeff@lokinet.io} --sign --detach $out &> /dev/null && gpg --verify $out.sig)
