// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "NaturalTime",
    platforms: [
        .iOS(.v13), .macOS(.v12)
    ],
    products: [
        .library(name: "NaturalTime", targets: ["NaturalTime"]) // public module name without "Core"
    ],
    targets: [
        // C target that compiles the core C sources directly from the repo root
        .target(
            name: "CNaturalTime",
            path: ".",
            exclude: [
                ".github", "build", "tests", "packages", "CMakeLists.txt", "README.md"
            ],
            sources: [
                "src/natural_time.c",
                "vendor/astronomy_c/astronomy.c"
            ],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("vendor/astronomy_c"),
                .define("NTC_VERSION", to: "swift")
            ]
        ),
        // Swift-friendly wrapper over the C API (kept in packages/ios folder)
        .target(
            name: "NaturalTime",
            dependencies: ["CNaturalTime"],
            path: "packages/ios/Sources/NaturalTime"
        )
    ]
)


