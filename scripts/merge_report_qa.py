#!/usr/bin/env python3

import os
import sys
import json
import subprocess
from datetime import datetime
import logging

# Try to import slack_sdk, but don't fail if it's not available
try:
    from slack_sdk import WebClient
    from slack_sdk.errors import SlackApiError
    slack_available = True
except ImportError:
    slack_available = False

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('qa_report')

def get_git_info():
    """Get basic git information about the current branch and PR."""
    try:
        # Get current branch name
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            universal_newlines=True
        ).strip()
        
        # Get the last commit message
        commit_msg = subprocess.check_output(
            ["git", "log", "-1", "--pretty=%B"],
            universal_newlines=True
        ).strip()
        
        # Get the commit hash
        commit_hash = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            universal_newlines=True
        ).strip()
        
        return {
            "branch": branch,
            "commit_message": commit_msg,
            "commit_hash": commit_hash
        }
    except subprocess.CalledProcessError as e:
        logger.error(f"Error getting git info: {e}")
        return {
            "branch": "unknown",
            "commit_message": "Could not retrieve commit message",
            "commit_hash": "unknown"
        }

def get_github_event_info():
    """Get information from GitHub event payload if available."""
    event_path = os.environ.get('GITHUB_EVENT_PATH')
    if not event_path or not os.path.exists(event_path):
        return {}
    
    try:
        with open(event_path, 'r') as f:
            event = json.load(f)
        
        # Handle PR event
        if 'pull_request' in event:
            pr = event['pull_request']
            return {
                'pr_number': pr.get('number'),
                'pr_title': pr.get('title'),
                'pr_url': pr.get('html_url'),
                'author': pr.get('user', {}).get('login'),
                'base_branch': pr.get('base', {}).get('ref'),
                'head_branch': pr.get('head', {}).get('ref')
            }
        
        # Handle push event
        if 'commits' in event:
            return {
                'push_ref': event.get('ref'),
                'commits': len(event.get('commits', [])),
                'author': event.get('pusher', {}).get('name')
            }
            
        return {}
    except Exception as e:
        logger.error(f"Error parsing GitHub event: {e}")
        return {}

def send_slack_message():
    """Send QA report to Slack."""
    if not slack_available:
        logger.warning("slack_sdk not available. Can't send message.")
        return False
    
    slack_token = os.environ.get('SLACK_TOKEN')
    if not slack_token:
        logger.warning("No Slack token provided. Can't send message.")
        return False
    
    git_info = get_git_info()
    gh_info = get_github_event_info()
    
    # Combine both sources of information
    info = {**git_info, **gh_info}
    
    client = WebClient(token=slack_token)
    
    # Create a meaningful message
    message = [
        "*QA Report for Flipper-k8s*",
        f"*Branch:* {info.get('branch', 'unknown')}",
        f"*Commit:* {info.get('commit_hash', 'unknown')}"
    ]
    
    if 'pr_number' in info:
        message.append(f"*PR:* <{info.get('pr_url', '#')}|#{info.get('pr_number')} {info.get('pr_title')}>")
    
    message.append(f"*Author:* {info.get('author', 'unknown')}")
    message.append(f"*Timestamp:* {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    try:
        # Use a test channel - update this to your QA channel
        response = client.chat_postMessage(
            channel="#testing",  # Update with your actual channel
            text="\n".join(message)
        )
        logger.info(f"Message sent: {response['ts']}")
        return True
    except SlackApiError as e:
        logger.error(f"Error sending message: {e}")
        return False

if __name__ == "__main__":
    logger.info("Starting QA report generation...")
    
    if send_slack_message():
        logger.info("QA report sent successfully.")
    else:
        logger.warning("Failed to send QA report.")
        # Don't fail the workflow if we can't send the message
    
    sys.exit(0)
