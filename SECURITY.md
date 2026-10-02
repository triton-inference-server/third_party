<!--
# Copyright 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

# Security Policy

## Reporting a Vulnerability

**Do not report security vulnerabilities through public GitHub issues,
discussions, or pull requests.**

To report a potential security vulnerability in this repository or any other
NVIDIA product, use one of the following channels:

1. **NVIDIA Vulnerability Disclosure Program** (preferred):
   https://www.nvidia.com/en-us/security/
2. **Email:** [psirt@nvidia.com](mailto:psirt@nvidia.com). Encrypt sensitive
   reports with NVIDIA's public PGP key:
   https://www.nvidia.com/en-us/security/pgp-key
3. **GitHub Private Vulnerability Reporting:** use the "Report a
   vulnerability" button on this repository's Security tab, if enabled.

**OEM partners should contact their NVIDIA Customer Program Manager.**

Please include:

1. Product name and version, branch, or commit that contains the vulnerability
2. Type of vulnerability (for example code execution, denial of service,
   buffer overflow)
3. Step-by-step instructions to reproduce the issue
4. Proof-of-concept or exploit code, if available
5. Potential impact, including how an attacker could exploit the issue

NVIDIA PSIRT acknowledges reports, assesses severity, coordinates a fix and
disclosure timeline with the reporter, and publishes Security Bulletins at
https://www.nvidia.com/en-us/security/.

Vulnerabilities in the upstream projects listed below (for example curl,
gRPC, libevent) should also be reported to those projects. Reporting to NVIDIA
PSIRT first is welcome when the issue affects Triton Inference Server.

## Security Architecture & Context

**Project:** Triton Third-Party Packages. This repository holds the build
definitions and patched sources for third-party libraries that Triton
Inference Server must modify before use.

**Software classification:** Library / build tooling (not a standalone
network service). Nothing in this repository is run as a service. It produces
statically or dynamically linked dependencies for Triton components.

**Contents:**

- `CMakeLists.txt`: a CMake `ExternalProject` superbuild that fetches pinned
  upstream releases from GitHub (for example curl, gRPC, protobuf, abseil,
  re2, c-ares, libevent, nlohmann/json, prometheus-cpp, crc32c,
  google-cloud-cpp, aws-sdk-cpp, azure-sdk-for-cpp, azure-iot-sdk-c,
  opentelemetry-cpp) and installs them under
  `TRITON_THIRD_PARTY_INSTALL_PREFIX`.
- `libevhtp/`: a vendored, patched copy of libevhtp (an event-driven HTTP
  server library written in C), built with SSL disabled
  (`EVHTP_DISABLE_SSL=ON`) and used by Triton's HTTP endpoint.
- `cnmem/`: a vendored, patched copy of the cnmem GPU memory pool library,
  with the changes recorded as `*.patch` files.
- `tools/patch.py` and `tools/install_src.py`: helper scripts that apply
  patches and copy source trees at build time.

**Primary security responsibility:** keeping the pinned third-party
dependency versions current and free of known vulnerabilities, and ensuring
local patches do not weaken the upstream code.

**Key security boundaries and interfaces:**

- Build time: network fetches of upstream source by tag or commit.
- Run time (consumers): parsing of untrusted HTTP requests through the
  vendored libevhtp and its parser (`libevhtp/libevhtp/parser.c`,
  `evhtp.c`); GPU memory management through cnmem.

**Repository Exposure Classification:** Public. Basis: the repository is
publicly visible on GitHub.

**Service Exposure Classification:** Internal-Sensitive (confidence: medium).
Basis: the repository is a build-time dependency source and not a deployed
service; its code reaches network-facing deployments only through Triton
Inference Server builds. These classifications are descriptive and are not
official NVIDIA labels.

## Threat Model

1. **Memory-safety flaws in vendored libevhtp:** The HTTP request parser and
   connection handling in `libevhtp/libevhtp/` are C code that processes
   attacker-controlled bytes when linked into Triton's HTTP endpoint. Parser
   or buffer-handling defects could lead to denial of service or memory
   corruption in the consuming server. The vendored copy does not receive
   upstream fixes automatically.
2. **Known vulnerabilities in pinned upstream dependencies:** The superbuild
   pins fixed versions of curl, gRPC, protobuf, libevent, c-ares, and the
   cloud SDKs. Pinned versions age, and disclosed CVEs in those versions
   propagate into every Triton component that builds against this repository
   until the pins are updated.
3. **Supply-chain compromise at fetch time:** `ExternalProject_Add` clones
   upstream repositories over HTTPS by tag or commit at build time. A moved
   or retagged upstream tag, a compromised upstream repository, or a network
   attacker defeating TLS could inject malicious code into the build.
   Tag-pinned entries are more exposed than commit-pinned ones.
4. **Patch tampering or silent drift:** Local modifications are carried as
   patch files (`cnmem/*.patch`) and applied by `tools/patch.py`, and a
   patched copy of libevhtp is vendored in-tree. A malicious or mistaken
   change to these files alters the behavior of security-relevant code
   without an upstream review trail.
5. **Unsafe file handling in build helpers:** `tools/install_src.py` removes
   and recreates a destination directory and copies a source tree with
   `shutil.rmtree` and `shutil.copytree(symlinks=True)`. A mis-set
   `--dest` or `--src` argument, or a symlink inside a source tree, could
   delete or expose files outside the intended location.

## Critical Security Assumptions

- **Build environment is trusted.** The build host, compilers, CMake, Python,
  and network path to GitHub are assumed uncompromised. This repository does
  not verify checksums or signatures of fetched upstream sources.
- **Upstream tags are immutable.** Pinned Git tags are assumed to keep
  pointing at the same reviewed commit.
- **libevhtp runs without TLS.** The vendored libevhtp is built with SSL
  disabled. Consumers are assumed to terminate TLS externally or to operate
  on a trusted network, and to apply their own request size limits,
  timeouts, and authentication.
- **Consumers validate input.** Code in this repository does not validate
  application-level input. Triton components that link these libraries are
  responsible for validation, authorization, and resource limits.
- **Patches are applied exactly as reviewed.** The patch tooling assumes the
  target files match the expected upstream content and fails if they do not.
  It does not authenticate the patch source.
- **Maintainers review dependency updates.** Version bumps in
  `CMakeLists.txt` and changes to vendored directories are assumed to go
  through code review and the repository's pre-commit and CodeQL checks.

## Supported Versions

Security fixes are made on the default branch and on the currently supported
Triton release branches. Older release branches are not maintained.
