# Legal and development policy

Revenge Native is an independent compatibility research project for Windows. Its purpose is to investigate lawful interoperability with Xbox Series X|S software. It is not affiliated with or endorsed by Microsoft, Xbox, any GPU vendor or any game publisher.

## Scope and boundaries

- The project accepts independently written implementations and appropriately licensed third-party contributions with documented provenance.
- Use public technical documentation, self-authored fixtures and other materials that you are authorised to access, analyse and use for this purpose. Permission to possess a file does not necessarily permit reverse engineering or redistribution.
- Do not contribute or distribute proprietary Microsoft or publisher code, console firmware, BIOS, confidential SDK headers or libraries, encryption keys, copyrighted game assets or commercial executables without the applicable rights and permissions.
- Revenge does not accept implementations designed to decrypt protected retail packages, circumvent DRM or entitlement checks, extract console secrets, defeat the security hypervisor or bypass authentication. Interoperability work must preserve applicable security and licensing requirements.
- Do not use official Xbox logos, dashboard artwork or other third-party brand assets without permission. Product and company names are used descriptively; trademarks remain the property of their respective owners.

## Implementation and provenance

The Native branch is an independent C++ rewrite of an earlier Rust-based experiment. It is not represented as a formally audited, two-team clean-room implementation. Earlier branches used VIctory-derived code and remain subject to the applicable original licence and attribution obligations.

For each new interface, record the public reference or authorised observation, describe the required behaviour independently, implement against that description and validate with shareable test fixtures. Where restricted or non-public materials would be involved, stop and obtain appropriate permissions and legal advice before proceeding. For higher-risk interface work, maintain documented separation between specification and implementation personnel where feasible. See [CONTRIBUTING.md](CONTRIBUTING.md) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The public Native repository must not contain raw game-derived diagnostic dumps or confidential material. Share only independently reproducible or explicitly authorised results; redact sensitive paths, identifiers and data. Keep any private research records only where authorised.

## Applicable law and agreements

Interoperability exceptions under laws such as Australia's Copyright Act 1968 and the US Copyright Act are conditional, jurisdiction-specific and do not automatically permit circumvention or redistribution. SDK agreements, confidentiality obligations, patents and trademarks are separate considerations. Follow the terms applicable to any SDK or test software. This policy is a project boundary, not a legal opinion or a guarantee that all uses or contributions are lawful.

## Distribution and licence

Original Revenge Native source is offered under **GPL-3.0-or-later**; see [LICENSE](LICENSE). This licence applies only to material the relevant rights holders are entitled to license. It does not confer rights to third-party games, SDKs, trademarks or platform services. Distributed binaries and modifications must comply with the GPL, including the applicable corresponding-source and notice requirements. Preserve all third-party notices and terms.

Before publishing a release, review the source, archive contents, dependencies, provenance records and any prior Git history for unauthorised material. If an issue is discovered, stop distributing the affected material until it is resolved. For a confidential rights or security concern, contact the repository maintainer privately where possible; do not attach restricted material to a public issue.

Obtain advice from an appropriately qualified intellectual-property lawyer before introducing legally sensitive functionality or distributing material of uncertain provenance.
