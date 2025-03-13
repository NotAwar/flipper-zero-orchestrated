# Flipper Zero Container Examples

This directory contains example pod manifests for the Flipper Zero containerization system.

## Manifest Format

The manifest files use a simple key-value format optimized for minimal memory usage during parsing.

### File Structure

The manifest files are organized into sections, each section representing a different aspect of the container configuration. Each section contains key-value pairs that define specific settings for that aspect.

### Example Manifest

Below is an example of a simple manifest file:

```
[container]
name = "example-container"
image = "example-image:latest"

[resources]
cpu = "500m"
memory = "256Mi"

[network]
port = 8080
protocol = "TCP"
```

### Key-Value Pairs

Each key-value pair in the manifest file is separated by an equals sign (`=`). Keys and values are case-sensitive and must be enclosed in double quotes if they contain spaces or special characters.

### Sections

The manifest file is divided into sections, each section starting with a section header enclosed in square brackets (`[]`). The section header defines the aspect of the container configuration that the section represents.

### Comments

Comments can be added to the manifest file by starting a line with a hash symbol (`#`). Comments are ignored during parsing and can be used to provide additional information or explanations within the manifest file.

