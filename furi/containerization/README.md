# Flipper Zero Container Runtime

This module implements a lightweight container runtime system for Flipper Zero inspired by Kubernetes but designed for resource-constrained environments.

## Features

- **Air-gapped operation**: Works entirely offline with no network connectivity required
- **Resource limitations**: Memory limits, CPU time share, and thread limits per container
- **Auto-restart**: Automatically restart crashed containers
- **Manifest-based deployment**: Use declarative configuration for multi-container applications
- **Service discovery**: Find and connect to services by name/namespace

## Components

- **ContainerRuntime**: Core runtime for container lifecycle management
- **ServiceRegistry**: Service discovery mechanism
- **PodManifest**: Format for defining multi-container applications

## CLI Commands

Control containers from the Flipper Zero CLI using `kubectl` commands:

