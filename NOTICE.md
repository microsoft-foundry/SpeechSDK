# NOTICE

This software incorporates material from the projects listed below. Microsoft
makes the associated copyright notices and license terms available below for
informational and attribution purposes.

---

## Components distributed in this repository

### Azure C Shared Utility (Microsoft fork)
- License: MIT
- Copyright (c) Microsoft Corporation
- Upstream: https://github.com/Azure/azure-c-shared-utility
- Location in repo: `source/core/external/azure-c-shared-utility/`

A Speech SDK specific fork of this Microsoft-owned library is vendored in this
repository and distributed under the MIT License:

    MIT License

    Copyright (c) Microsoft Corporation.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

---

## Components referenced but NOT distributed in this repository

### OpenSSL (Version 3.x)
- License: Apache License, Version 2.0
- Copyright (c) The OpenSSL Project Authors. All Rights Reserved.
- Project: https://www.openssl.org/

OpenSSL is **not** included or redistributed in this repository. It is a
build-time dependency that the consumer installs separately and that is linked
to the SDK at build time. This entry is provided for attribution and
informational purposes only.
