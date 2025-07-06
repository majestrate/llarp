LLARP now talks to systemd directly via sdbus to set up DNS, but in order for this to work the
user running llarpd (assumed `_llarpd` in these example files) needs permission to set dns servers
and domains.

To set up the permissions:

- If lokinet is running as some user other than `_llarpd` the change the `_llarpd` username inside
  `llarpd.rules` and `llarpd.pkla`.

- If on a Debian or Debian-derived distribution (such as Ubuntu) using polkit 105,
  copy `llarpd.pkla` to `/var/lib/polkit-1/localauthority/10-vendor.d/llarpd.pkla` (for a distro
  install) or `/etc/polkit-1/localauthority.conf.d/` (for a local install).

- Copy `llarpd.rules` to `/usr/share/polkit-1/rules.d/` (distro install) or `/etc/polkit-1/rules.d`
  (local install).

Make use of it by switching to systemd-resolved:
```
sudo ln -sf /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
sudo systemctl enable --now systemd-resolved
```
