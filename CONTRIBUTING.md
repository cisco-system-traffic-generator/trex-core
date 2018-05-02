# Guidance on how to Contribute

> All contributions to this project will be released under the Apache 2.0 License.
> By submitting a pull request or filing a bug, issue, or
> feature request, you are agreeing to comply with this waiver of copyright interest.
> Details can be found in our [LICENCE](LICENSE).


There are several ways to help:
 - ask questions and participate in discussions in our google group
 - use the issue tracker to submit issues and enhancements
 - change the codebase for code and documentation fixes or improvements


## Asking Questions and Participating in Discussions

Use the TRex traffic generator group to ask and answer questions. Feel free to
participate! This is a great way to connect with the developers of the project
as well as others who are interested in this solution.
 - https://groups.google.com/forum/#!forum/trex-tgn

If you prefer using e-mail, the mailer is:
 - trex-tgn@googlegroups.com

## Using the Issue Tracker

We have two issue trackers:
 1. github (https://github.com/cisco-system-traffic-generator/trex-core/issues) [PREFERRED]
 2. youtrack (https://trex-tgn.cisco.com/youtrack/issues) [LEGACY]

Use the github issue tracker to suggest new feature requests and report bugs.
This is our preferred issue tracker going forward.

There is a backlog of issues and enhancements in the youtrack issue tracker.

Check either issue tracker to find ways to contribute. Find a bug or a feature,
mention in the issue that you will take on that effort, then follow the
_Changing the Codebase_ guidance below.


## Changing the Codebase

Make code and documentation changes directly in this git repository.

General Best Practices:
 1. fork this repository
 2. make changes in your own fork on a narrowly focussed branch
 3. squash commits
 4. submit a pull-request

Active developers assigned to the project will triage and moderate pull requests.

Test Coverage:
 - All new code should have associated unit tests that validate implemented
features and the presence or lack of defects.

Style:
 - Code should follow any stylistic and architectural guidelines prescribed
by the project. In the absence of such guidelines, mimic the styles and patterns
in the existing codebase.

Commit Notes:
 - Always use git's `-s (--signoff)` flag to append the legal sign off required.
This applies to all pull requests. If making documentation changes directly in
gitlab, you can manually append the signoff note in the commit message.
> Signed-off-by: Full Name \<e-mail\>
 - See `man git-commit` for more details.
