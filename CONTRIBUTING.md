# Contributing

<!-- Include start contributing intro -->

## Ubuntu WSL and Ubuntu Pro for WSL

The documentation for Ubuntu WSL and Ubuntu Pro for WSL are currently
in the process of being merged. These contribution guidelines refer
to both products.

Contributions to Ubuntu WSL and Ubuntu Pro for WSL are welcomed and encouraged.

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

We take our community seriously and hold ourselves and other contributors to high standards of communication. By participating and contributing, you agree to uphold the Ubuntu Community [Code of Conduct](https://ubuntu.com/community/code-of-conduct).

## Getting started

The source code for Ubuntu WSL and Ubuntu Pro for WSL can be found on GitHub:

- [Ubuntu WSL repo](https://github.com/ubuntu/WSL) 
- [Ubuntu Pro for WSL repo](https://github.com/canonical/ubuntu-pro-for-wsl) 

### Issues with WSL should be directed to Microsoft's WSL project

<!-- TODO: a breakdown of responsibilities or example issues (Ubuntu/Microsft) could be helpful here -->

We accept any contributions relating to Ubuntu WSL and Ubuntu Pro for WSL.
However, we do not directly maintain WSL itself, which is a Microsoft product.
If you have identified a problem or bug in WSL then file an issue in
[Microsoft's WSL project repository](https://github.com/microsoft/WSL/issues/).

If you are unsure whether your problem relates to an Ubuntu project or the Microsoft project then familiarise yourself with their documentation.

- [Ubuntu WSL docs](https://canonical-ubuntu-wsl.readthedocs-hosted.com/en/latest/)
- [Microsoft WSL docs](https://learn.microsoft.com/en-us/windows/wsl/)

At this point, if you are still not sure, try to contact a maintainer of one of the projects who will advise you where best to submit your Issue.

### How to contribute

Contributions are made via Issues and Pull Requests (PRs). A few general guidelines that cover both:

* Use the advisories page of the repository and not a public bug report to
report security vulnerabilities. For Ubuntu WSL, please use [launchpad private
bugs](https://bugs.launchpad.net/ubuntuwsl/+filebug) which is
monitored by our security team. On an Ubuntu machine, it’s best to use
`ubuntu-bug wsl` to collect relevant information.
<!-- QUERY: is `wsl` the correct argument here???. -->
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

### Required dependencies for UP4W

You'll need a Windows Machine with the following applications installed:

* Windows Subsystem for Linux
* Ubuntu-24.04
* Visual Studio Community 2019 or above
* Go
* Flutter

### Building and running the binaries for UP4W

For building, you can use the following two scripts:

* [Build the Windows Agent](https://github.com/canonical/ubuntu-pro-for-wsl/blob/main/tools/build/build-deb.sh)
* [Build the Wsl Pro Service](https://github.com/canonical/ubuntu-pro-for-wsl/blob/main/tools/build/build-appx.ps1)

Note that you'll need to [create a self-signing certificate](https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing) to build the Windows Agent.

## Contributor License Agreement

It is requirement that you sign the [Contributor License
Agreement](https://ubuntu.com/legal/contributors) in order to contribute.
You only need to sign this once and if you have previously signed the
agreement when contributing to other Canonical projects you will not need to
sign it again.

An automated test is executed on PRs to check if it has been accepted.

Please refer to the licences for Ubuntu WSL and Ubuntu Pro for WSL below:

- [Ubuntu WSL](https://github.com/ubuntu/WSL/blob/main/LICENSE).
- [Ubuntu Pro for WSL](https://github.com/canonical/ubuntu-pro-for-wsl/blob/main/LICENSE).

## Contributing to the docs

The documentation for Ubuntu WSL and Ubuntu Pro for WSL is maintained [here](https://github.com/ubuntu/WSL/tree/main/docs).

Our goal is to provide documentation that gives users the information that they need to get what they need from Ubuntu WSL.

You can contribute to the documentation in various different ways. If you are not a developer but want to help make the product better then helping us to improve the documentation is a way to achieve that.

At the top of each page in the documentation, you will find a feedback button.
Clicking this button will open an Issue submission page in the Ubuntu WSL GitHub repo.
A template will automatically be loaded that you can modify before submitting the Issue.

You can also find a pencil icon for editing the page on GitHub, which will open up the source file in GitHub so that you can make changes before committing them and submitting a PR.
This can be a good option if you want to make a small change, e.g., fixing a single typo.

Lastly, at the bottom of the page you will find various links, including a link to the Discourse forum for Ubuntu WSL, where you can ask questions and participate in discussions.

### Types of contribution

<!-- TODO: if PR title conventions are in place these should be specified here -->

Some common contributions to documentation are:

- Add or update documentation for new features or feature improvements by submitting a PR
- Add or update documentation that clarifies any doubts you had when working with the product by submitting a PR
- Request a fix to the documentation, by opening an issue on GitHub.
- Post a question or suggestion on the forum.

### Automatic documentation checks

Automatic checks will be run on any PR relating to documentation to verify the spelling, the validity of links, correct formatting of the Markdown files and the use of inclusive language.

You should run these tests locally before submitting a PR by running the following commands:

- Check the spelling: `make spelling`
- Check the validity of links: `make linkcheck`
- Check for inclusive language: `make woke`

Doing these checks locally is good practice. You are less likely to run into
failed CI checks after your PR is submitted and the reviewer of your PR can
more quickly focus on the contribution you have made.

## Getting Help

Join us in the [Ubuntu Community](https://discourse.ubuntu.com/c/wsl/27) and post your question there with a descriptive tag.

<!-- Include end contributing main -->
