# Development Tools

This directory contains various utilities to assist with Flipper Zero firmware development.

## TODO Tracker

`todo_tracker.py` is a tool for scanning the codebase for TODOs, FIXMEs, and similar annotations. It generates reports to help manage technical debt.

### Usage

```bash
# Generate reports in all formats (markdown, JSON, CSV)
./todo_tracker.py --root /path/to/firmware --output todo_report

# Generate only a markdown report
./todo_tracker.py --format md --output todo_report
```

### Example Output

The tool generates structured reports that organize TODOs by ticket reference:
- TODOs with ticket references (e.g., FL-3528) are grouped together
- TODOs without ticket references are listed separately
- Summary statistics are provided

This helps with prioritizing and tracking technical debt systematically.
