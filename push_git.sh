#!/usr/bin/env bash
# ==============================================================================
# push_git.sh - Commit, Tag, and Push Script for LPC_Revive
# ==============================================================================
# Usage:
#   ./push_git.sh -tag "<tag_name>" [-m "<commit_message>"]
#
# Examples:
#   ./push_git.sh -tag "v1.0.1"
#   ./push_git.sh -tag "v1.0.1" -m "Fixed HTTP socket prompt and 4G APN handling"
# ==============================================================================

set -e

# Terminal colors
BOLD='\033[1m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Navigate to repository root
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [ ! -d ".git" ]; then
    echo -e "${RED}[ERROR] This directory is not a Git repository (.git not found)!${NC}"
    exit 1
fi

TAG_NAME=""
COMMIT_MSG=""
FORCE_TAG=0

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -tag|--tag|-t)
            TAG_NAME="$2"
            shift 2
            ;;
        -tag=*|--tag=*)
            TAG_NAME="${1#*=}"
            shift 1
            ;;
        -m|-message|--message)
            COMMIT_MSG="$2"
            shift 2
            ;;
        -m=*|--message=*)
            COMMIT_MSG="${1#*=}"
            shift 1
            ;;
        -f|--force)
            FORCE_TAG=1
            shift 1
            ;;
        -h|--help)
            echo -e "${BOLD}Usage:${NC}"
            echo "  ./push_git.sh -tag \"<tag_name>\" [-m \"<commit_message>\"] [-f]"
            echo ""
            echo -e "${BOLD}Options:${NC}"
            echo "  -tag, --tag <tag>     Tag name to create and push (e.g. v1.0.1, office-sync-1)"
            echo "  -m, --message <msg>   Optional commit message"
            echo "  -f, --force           Force overwrite tag if it already exists"
            echo "  -h, --help            Show this help message"
            exit 0
            ;;
        *)
            # If positional argument does not start with -, treat as tag name if empty
            if [ -z "$TAG_NAME" ] && [[ ! "$1" =~ ^- ]]; then
                TAG_NAME="$1"
                shift 1
            else
                echo -e "${YELLOW}[WARNING] Unknown argument: $1${NC}"
                shift 1
            fi
            ;;
    esac
done

# If no tag was specified, prompt interactively
if [ -z "$TAG_NAME" ]; then
    echo -e "${CYAN}=== LPC_Revive Git Push & Tag Utility ===${NC}"
    echo -n "Enter tag name (e.g. v1.0.1, office-nov03): "
    read -r TAG_NAME
fi

if [ -z "$TAG_NAME" ]; then
    echo -e "${RED}[ERROR] Tag name cannot be empty. Aborted.${NC}"
    exit 1
fi

# Clean tag name (trim spaces)
TAG_NAME="$(echo "$TAG_NAME" | xargs)"

# Default commit message if not provided
if [ -z "$COMMIT_MSG" ]; then
    TIMESTAMP="$(date '+%Y-%m-%d %H:%M:%S')"
    COMMIT_MSG="Release ${TAG_NAME} (${TIMESTAMP})"
fi

# Determine current branch (default to main)
CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")"
if [ "$CURRENT_BRANCH" = "HEAD" ]; then
    CURRENT_BRANCH="main"
fi

echo -e "${CYAN}======================================================${NC}"
echo -e "${BOLD}LPC_Revive: Preparing Push & Tag${NC}"
echo -e "  Repository: ${BOLD}${ROOT_DIR}${NC}"
echo -e "  Branch:     ${GREEN}${CURRENT_BRANCH}${NC}"
echo -e "  Tag:        ${GREEN}${TAG_NAME}${NC}"
echo -e "  Message:    ${BOLD}${COMMIT_MSG}${NC}"
echo -e "${CYAN}======================================================${NC}"

# Check for uncommitted changes or untracked files
STATUS_OUTPUT="$(git status --porcelain)"

if [ -n "$STATUS_OUTPUT" ]; then
    echo -e "\n${YELLOW}1. Staging and committing changes...${NC}"
    git add -A
    git commit -m "$COMMIT_MSG"
    echo -e "${GREEN}   Commit created successfully.${NC}"
else
    echo -e "\n${GREEN}1. Working tree is clean (no changes to commit).${NC}"
fi

# Handle Tagging
echo -e "\n${YELLOW}2. Creating tag '${TAG_NAME}'...${NC}"
if git rev-parse "$TAG_NAME" >/dev/null 2>&1; then
    if [ "$FORCE_TAG" -eq 1 ]; then
        echo -e "${YELLOW}   Tag '${TAG_NAME}' already exists locally. Overwriting (--force)...${NC}"
        git tag -a "$TAG_NAME" -m "$COMMIT_MSG" -f
    else
        echo -e "${YELLOW}   Tag '${TAG_NAME}' already exists locally.${NC}"
        echo -n "   Do you want to update it to the latest commit? [y/N]: "
        read -r CONFIRM
        if [[ "$CONFIRM" =~ ^[Yy]$ ]]; then
            git tag -a "$TAG_NAME" -m "$COMMIT_MSG" -f
            FORCE_TAG=1
        else
            echo -e "${RED}   Aborted to avoid overwriting existing tag.${NC}"
            exit 1
        fi
    fi
else
    git tag -a "$TAG_NAME" -m "$COMMIT_MSG"
    echo -e "${GREEN}   Tag '${TAG_NAME}' created.${NC}"
fi

# Pushing Branch and Tag
echo -e "\n${YELLOW}3. Pushing branch '${CURRENT_BRANCH}' to remote 'origin'...${NC}"
git push origin "$CURRENT_BRANCH"

echo -e "\n${YELLOW}4. Pushing tag '${TAG_NAME}' to remote 'origin'...${NC}"
if [ "$FORCE_TAG" -eq 1 ]; then
    git push origin "$TAG_NAME" --force
else
    git push origin "$TAG_NAME"
fi

echo -e "\n${GREEN}======================================================${NC}"
echo -e "${BOLD}${GREEN}✔ Successfully pushed to GitHub!${NC}"
echo -e "  Remote:  https://github.com/satishvspatika/LPC_Revive"
echo -e "  Branch:  ${CURRENT_BRANCH}"
echo -e "  Tag:     ${TAG_NAME}"
echo -e "${GREEN}======================================================${NC}"
echo -e "To retrieve this exact version on your other laptop/system, run:"
echo -e "  ${BOLD}./get_git.sh -tag \"${TAG_NAME}\"${NC}\n"
