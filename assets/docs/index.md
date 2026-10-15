# Yodau contracts

Yodau shares one capture/processing model and validated configuration between its
GUI and headless application. Desktop shell selection changes integration, not
processing or data ownership.

- [Processing algorithms](algorithms.md)
- [Qt and KDE desktop profiles](desktop_profiles.md)
- [Android lifecycle and configuration](android.md)
- [Headless daemon and exclusive output](headless_daemon.md)
- [Persistence and diagnostic ownership](persistence.md)

Only current internal formats and interfaces are supported. Malformed input is
rejected; a rejected import does not partially replace live configuration.
