# Azure Speech in Foundry Tools - Speech SDK source-code

This repository provides the open-sourced version of the Azure Speech in Foundry Tools Speech SDK source-code core functionality.

This enables customers who require building binaries from sources by making the Speech SDK available in source form.

The following limitations apply:
* Only C++ source code and APIs are available.
* Functionality of any official extension packages (Microsoft.CognitiveServices.Speech.Extension.* on nuget.org) is not included.
  This means the following features are not supported:
  * Audio processing using the Microsoft Audio Stack
  * Compressed input audio
  * Direct access to audio devices (microphone, headphone/loudspeaker)
  * Embedded speech
  * Keyword recognition
* Mixing binaries built from sources in this repository and binaries from the official SDK releases is not guaranteed to work.

For build setup and supported platforms, refer to the [GitHub Actions workflow](.github/workflows/cmake-multi-platform.yml) in this repository.

Customers who require the full functionality of the Speech SDK and/or want to use a compiled version in one of the many languages we provide, please refer to the documentation linked below.

## Documentation

- [Azure Speech - Speech SDK](https://learn.microsoft.com/azure/ai-services/speech-service/speech-sdk)
- **Data Collection.** The software may collect information about you and your use of the software and send it to Microsoft. Microsoft may use this information to provide services and improve our products and services. You may turn off the telemetry as described in the repository. There are also some features in the software that may enable you and Microsoft to collect data from users of your applications. If you use these features, you must comply with applicable law, including providing appropriate notices to users of your applications together with a copy of Microsoft's privacy statement. Our privacy statement is located at https://go.microsoft.com/fwlink/?LinkID=824704. You can learn more about data collection and use in the help documentation and our privacy statement. Your use of the software operates as your consent to these practices.

  - To disable telemetry, you can call the following API:
    ```cpp
    speechConfig->SetProperty("SPEECH-TelemetryDataEnabled", "false");
    ```
    However, we strongly recommend that you keep telemetry enabled. It will transmit information about your platform and the performance of the Speech Service. It can be used to tune the service, monitor service performance and stability, and might help us analyze reported problems. Without telemetry enabled, it is not possible for us to do any form of detailed analysis in case of a support request.

## Versioning and API stability

This repository follows the same versioning scheme as the official Azure Speech SDK binary releases. Public APIs are backwards compatible across releases unless explicitly marked as deprecated or breaking in release notes.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

Please note that this project follows the [Microsoft Open Source Code of Conduct](CODE_OF_CONDUCT.md).

## Resources

- [Support](SUPPORT.md) - Get help and file issues
- [Security](SECURITY.md) - Security policy and reporting vulnerabilities

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md) file for details.

Copyright (c) Microsoft Corporation. All rights reserved.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.
