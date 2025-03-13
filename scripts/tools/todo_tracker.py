#!/usr/bin/env python3
"""
TODO Tracking Tool for Flipper Zero firmware.
This script scans the codebase for TODO, FIXME, and similar annotations,
categorizes them, and generates reports.
"""

import os
import re
import argparse
import json
import csv
from datetime import datetime
from collections import defaultdict

# Regex patterns for finding annotations
TODO_PATTERN = re.compile(r'(?:\/\/|\/\*|\#|;)\s*(TODO|FIXME|HACK|XXX)(?:\s*\(?([A-Z]+-\d+)\)?)?:?\s*(.*?)(?=\n|$)', re.IGNORECASE)

class TodoTracker:
    def __init__(self, root_dir):
        self.root_dir = root_dir
        self.todos = []
        self.excluded_dirs = ['.git', 'node_modules', 'build', '.vscode/example']
        self.included_extensions = ['.c', '.h', '.cpp', '.py', '.js', '.ts', '.md', '.yml', '.scons']

    def is_excluded_path(self, path):
        for excluded in self.excluded_dirs:
            if excluded in path:
                return True
        return False

    def scan_file(self, file_path):
        _, ext = os.path.splitext(file_path)
        if ext not in self.included_extensions:
            return

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                line_number = 1
                for line in content.split('\n'):
                    matches = TODO_PATTERN.findall(line)
                    for match in matches:
                        annotation_type, ticket, description = match
                        if "-nofl" not in line:  # Skip items with -nofl
                            self.todos.append({
                                'file': os.path.relpath(file_path, self.root_dir),
                                'line': line_number,
                                'type': annotation_type.upper(),
                                'ticket': ticket,
                                'description': description.strip(),
                                'line_content': line.strip()
                            })
                    line_number += 1
        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    def scan_directory(self):
        for root, dirs, files in os.walk(self.root_dir):
            # Skip excluded directories
            dirs[:] = [d for d in dirs if not self.is_excluded_path(os.path.join(root, d))]
            
            for file in files:
                file_path = os.path.join(root, file)
                self.scan_file(file_path)

    def generate_markdown_report(self, output_path):
        # Group TODOs by ticket
        by_ticket = defaultdict(list)
        no_ticket = []
        
        for todo in self.todos:
            if todo['ticket']:
                by_ticket[todo['ticket']].append(todo)
            else:
                no_ticket.append(todo)

        with open(output_path, 'w', encoding='utf-8') as f:
            f.write("# TODO Tracking Report\n\n")
            f.write(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            f.write("## Summary\n\n")
            f.write(f"- Total TODOs: {len(self.todos)}\n")
            f.write(f"- With ticket references: {len(self.todos) - len(no_ticket)}\n")
            f.write(f"- Without ticket references: {len(no_ticket)}\n\n")
            
            # Write TODOs with tickets
            f.write("## TODOs with Ticket References\n\n")
            for ticket, items in by_ticket.items():
                f.write(f"### {ticket} ({len(items)} items)\n\n")
                for item in items:
                    f.write(f"- **{item['file']}:{item['line']}** ({item['type']}): {item['description']}\n")
                f.write("\n")
            
            # Write TODOs without tickets
            f.write("## TODOs without Ticket References\n\n")
            for item in no_ticket:
                f.write(f"- **{item['file']}:{item['line']}** ({item['type']}): {item['description']}\n")

    def generate_json_report(self, output_path):
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump({
                'generated_at': datetime.now().isoformat(),
                'total_count': len(self.todos),
                'items': self.todos
            }, f, indent=2)

    def generate_csv_report(self, output_path):
        with open(output_path, 'w', encoding='utf-8', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['File', 'Line', 'Type', 'Ticket', 'Description', 'Content'])
            for todo in self.todos:
                writer.writerow([
                    todo['file'],
                    todo['line'],
                    todo['type'],
                    todo['ticket'],
                    todo['description'],
                    todo['line_content']
                ])


def main():
    parser = argparse.ArgumentParser(description='Track TODOs in Flipper Zero firmware')
    parser.add_argument('--root', '-r', default=os.getcwd(), help='Root directory to scan')
    parser.add_argument('--output', '-o', default='todo_report', help='Output file prefix (without extension)')
    parser.add_argument('--format', '-f', choices=['md', 'json', 'csv', 'all'], default='all', help='Output format')
    
    args = parser.parse_args()
    
    tracker = TodoTracker(args.root)
    tracker.scan_directory()
    
    if args.format in ['md', 'all']:
        tracker.generate_markdown_report(f"{args.output}.md")
        print(f"Markdown report generated: {args.output}.md")
        
    if args.format in ['json', 'all']:
        tracker.generate_json_report(f"{args.output}.json")
        print(f"JSON report generated: {args.output}.json")
        
    if args.format in ['csv', 'all']:
        tracker.generate_csv_report(f"{args.output}.csv")
        print(f"CSV report generated: {args.output}.csv")


if __name__ == "__main__":
    main()
