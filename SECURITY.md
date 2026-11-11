# Security Policy

## Project status

VEYRA is pre-release software. It is not recommended for sensitive browsing, passwords, payments, or other important activity. Its security controls and update process are still being developed.

There are currently no stable releases. Security reports are most relevant to the latest code on the `main` branch and the latest official development build. Older development builds may no longer be supported.

## Reporting a vulnerability

**Please do not publish suspected security vulnerabilities in public GitHub Issues, Discussions, pull requests, videos, or other public channels.** Public disclosure before a fix is available could put VEYRA users at risk.

The preferred reporting route is GitHub's **Private vulnerability reporting** feature on this repository:

1. Open the repository's **Security** tab.
2. Select **Advisories**.
3. Choose **Report a vulnerability** and submit the report privately.

Please include a clear description of the affected VEYRA version or commit, the impact you believe is possible, and steps to reproduce the issue. Avoid including real personal data, passwords, access tokens, or other secrets in a report.

If the private reporting button is not available, do not post technical details publicly. Open a public issue containing only the sentence **"I need a private channel to report a security concern."** Do not include exploit details or sensitive information in that issue. The maintainer can then arrange a private reporting route.

We aim to acknowledge reports within five working days. Investigation and fixes may take longer, especially while VEYRA remains pre-release. Please allow time for a fix to be prepared and tested before making a report public.

## Scope

Security reports relevant to VEYRA include issues in the browser shell, tab and navigation handling, Chromium/CEF integration, sandbox configuration, download and file handling, permissions, certificate and TLS handling, update delivery, and the build or release pipeline.

Third-party Chromium and CEF vulnerabilities should be reported to their respective upstream projects as well. Please also tell us which VEYRA/CEF version is affected so we can assess whether VEYRA needs an update.

## Coordinated disclosure

Please give the maintainer a reasonable opportunity to investigate, prepare a fix, and coordinate a public advisory before disclosing a vulnerability. We will work with reporters in good faith, but cannot guarantee a particular fix timeline or reward.
