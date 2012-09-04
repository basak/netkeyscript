netkeyscript
============

This is a simple networking keyscript that can be used from crypttab(5) in
order to boot headless encrypted servers by supplying key material over the
network.

When run, it will bring `eth0` up if it is not up already, and then wait for a
IPv6 UDP packet containing the passphrase to be sent to the local link
multicast address. Once it arrives, the keyscript prints the received
passphrase to standard output and exits, allowing the system to decrypt the
disk with the supplied passphrase and continue.

Installation
-------------

On the headless server, install `netkeyscript` into `/usr/local/sbin/` and
modify `/etc/crypttab` to add a keyscript option. It should end up looking
something like this:

	md1_crypt UUID=01234567-89ab-cdef-0123-456789abcdef none luks,keyscript=/usr/local/sbin/netkeyscript

On Debian and Ubuntu, you need to update the initramfs after changing `crypttab`:

	sudo update-initramfs -u


Usage
-----

Once set up, use `netkeyscript-send.py` on your client like this when the
server is waiting for the passphrase:

	echo -n 'passphrase'|sudo python netkeyscript-send.py --iface=eth0

In this example, _eth0_ is the interface on your client machine to use to send
out the key.

Security Considerations
-----------------------

* The key is sent to the machine unencrypted. Anyone on your LAN will be able
  to pick it up. You should decide whether this is safe in your situation.
  You could use a temporary crossover cable directly as an alternative.

* If Mallory pretends to be your server (for example: by physically replacing
  it, or by intercepting network communication), he can fool you into handing
  over your passphrase.

Future Directions
-----------------

* Add network encryption for the passphrase itself based on a shared secret.
  This would protect against LAN-based snooping, but would still need to assume
  that the server has not been physically compromised, as the shared secret
  would need to be stored on the server unencrypted.
