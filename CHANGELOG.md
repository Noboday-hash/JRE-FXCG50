# Changelog

## Unreleased — M1 in progress

- Added bounded ZIP central-directory indexing shared by host and calculator, with stored-entry extraction and CRC checks.
- Added bounded main-section manifest parsing and a host archive listing command.
- Added malformed archive and manifest fixtures.
- Integrated pinned miniz tinfl for bounded raw DEFLATE extraction, including data-descriptor fixtures.
- Added bounded class format 45.3 parsing and host class metadata reporting.
- Added MIDlet declaration parsing, opcode usage scanning, member-reference listing, and `tools/inspect_jar.py`.

## Unreleased — M0 scaffold

- Created repository component layout, shared smoke contract, host harness, and fx-CG50 smoke add-in source.
- Added state and planning documents. No Java ME runtime features are implemented yet.
