# Contributing

<!-- Include start contributing intro -->

## Ubuntu on WSL distribution

Contributions to Ubuntu on WSL are welcomed and encouraged.

## Guidelines for your contributions

To ensure that making a contribution is a positive experience for both contributor and reviewer we ask that you read and follow these community guidelines.

This communicates that you respect our time as developers. We will return
that respect by addressing your issues, assessing proposed changes and
finalising your pull requests, as helpfully and efficiently as possible.

These are mostly guidelines, not rules. Use your best judgement and feel free to
propose changes to this document in a pull request.

<!-- Include end contributing intro -->

## Quicklinks

* [Code of conduct](#code-of-conduct)
* [Getting started](#getting-started)
* [Issues](#issues)
* [Pull requests](#pull-requests)
* [Contributing to the code](#contributing-to-the-code)
* [Contributing to the docs](#contributing-to-the-docs)
* [Contributor License Agreement](#contributor-license-agreement)
* [Getting kelp](#getting-help)

<!-- Include start contributing main -->

## Code of conduct

We take our community seriously and hold ourselves and other contributors to high standards of communication. By participating and contributing, you agree to uphold the Ubuntu Community [Code of Conduct](https://ubuntu.com/community/ethos/code-of-conduct).

## Getting started

The source code for Ubuntu WSL and Ubuntu Pro for WSL can be found on GitHub:

- [Ubuntu on WSL distribution](https://github.com/ubuntu/WSL)
- [Ubuntu Pro for WSL application](https://github.com/canonical/ubuntu-pro-for-wsl)

### Issues with WSL should be directed to Microsoft's WSL project

We accept any contributions relating to the Ubuntu on WSL distribution.
However, we do not directly maintain WSL itself, which is a Microsoft product.
If you have identified a problem or bug in WSL then file an issue in
[Microsoft's WSL project repository](https://github.com/microsoft/WSL/issues/).

If you are unsure whether your problem relates to an Ubuntu project or the Microsoft project then familiarise yourself with their documentation.

- [Ubuntu WSL docs](https://documentation.ubuntu.com/wsl/en/latest/)
- [Microsoft WSL docs](https://learn.microsoft.com/en-us/windows/wsl/)

At this point, if you are still not sure, try to contact a maintainer of one of the projects who will advise you where best to submit your Issue.

### How to contribute

Contributions are made via Issues and Pull Requests (PRs). A few general guidelines that cover both:

* Use the advisories page of the repository and not a public bug report to
report security vulnerabilities. 
* Search for existing Issues and PRs before creating your own.
* Give a friendly ping in the comment thread to the submitter or a contributor to draw attention if your issue is blocking — while we work hard to makes sure issues are handled in a timely manner it can take time to investigate the root cause. 
* Read [this Ubuntu discourse post](https://discourse.ubuntu.com/t/contribute/26) for resources and tips on how to get started, especially if you've never contributed before

### Issues

Issues should be used to report problems with the software, request a new feature or to discuss potential changes before a PR is created. When you create a new Issue, a template will be loaded that will guide you through collecting and providing the information that we need to investigate.

If you find an Issue that addresses the problem you're having, please add your own reproduction information to the existing issue rather than creating a new one. Adding a [reaction](https://github.blog/2016-03-10-add-reactions-to-pull-requests-issues-and-comments/) can also help be indicating to our maintainers that a particular problem is affecting more than just the reporter.

### Pull requests

PRs are always welcome and can be a quick way to get your fix or improvement slated for the next release. In general, PRs should:

* Only fix/add the functionality in question **OR** address wide-spread whitespace/style issues, not both.
* Add unit or integration tests for fixed or changed functionality.
* Address a single concern in the least number of changed lines as possible.
* Include documentation in the repo or on our [docs site](https://github.com/canonical/ubuntu-pro-for-wsl/wiki).
* Use the complete Pull Request template (loaded automatically when a PR is created).

For changes that address core functionality or would require breaking changes (e.g. a major release), it's best to open an Issue to discuss your proposal first. This is not required but can save time creating and reviewing changes.

In general, we follow the ["fork-and-pull" Git workflow](https://github.com/susam/gitpr):

1. Fork the repository to your own GitHub account.
1. Clone the fork to your machine.
1. Create a branch locally with a succinct yet descriptive name.
1. Commit changes to your branch.
1. Follow any formatting and testing guidelines specific to this repo.
1. Push changes to your fork.
1. Open a PR in our repository and follow the PR template so that we can efficiently review the changes.

> PRs will trigger unit and integration tests with and without race detection, linting and formatting validations, static and security checks, freshness of generated files verification. All the tests must pass before anything is merged into the main branch.

Once merged to the main branch, `po` files will be automatically updated and are therefore not necessary to update in the pull request itself, which helps minimise diff review.

## Contributing to the code

### About the test suite

The source code includes a comprehensive test suite made of unit and integration tests. All the tests must pass with and without the race detector.

Each module has its own package tests and you can also find the integration tests at the appropriate end-to-end (e2e) directory.

The test suite must pass before merging the PR to our main branch. Any new feature, change or fix must be covered by corresponding tests.

### Contributor License Agreement

It is requirement that you sign the [Contributor License
Agreement](https://ubuntu.com/legal/contributors) in order to contribute.
You only need to sign this once and if you have previously signed the
agreement when contributing to other Canonical projects you will not need to
sign it again.

An automated test is executed on PRs to check if it has been accepted.

Please refer to the licences for Ubuntu on WSL:

- [Ubuntu on WSL](https://github.com/ubuntu/WSL/blob/main/LICENSE).

## Contributing to the docs

> [!WARNING]
> The **documentation** for Ubuntu on WSL can be found at [https://documentation.ubuntu.com/wsl/](documentation.ubuntu.com/wsl).
> The source for that documentation can be found in the [Ubuntu Pro for WSL repo](https://github.com/canonical/ubuntu-pro-for-wsl).
> If you are interested in contributing to the documentation, please submit your Issues and Pull Requests there.

## Getting Help

Join us in the [Ubuntu Community](https://discourse.ubuntu.com/c/wsl/27) and post your question there with a descriptive tag.

<!-- Include end contributing main -->
