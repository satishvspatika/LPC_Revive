#!/usr/bin/env bash
# ==============================================================================
# get_git.sh - Retrieve, Pull, or Checkout Specific Tag from LPC_Revive
# ==============================================================================
# Usage:
#   ./get_git.sh                    # Pulls latest updates on current branch + fetches tags
#   ./get_git.sh -tag "<tag_name>"  # Fetches and checks out a specific tag
#   ./get_git.sh -list              # Lists all available tags in the repository
#
# Examples:
#   ./get_git.sh
#   ./get_git.sh -tag "v1.0.1"
#   ./get_git.sh -list
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
LIST_TAGS=0

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
        -l|-list|--list)
            LIST_TAGS=1
            shift 1
            ;;
        -h|--help)
            echo -e "${BOLD}Usage:${NC}"
            echo "  ./get_git.sh                   Retrieve latest updates on current branch"
            echo "  ./get_git.sh -tag \"<tag_name>\" Retrieve and checkout a specific tag"
            echo "  ./get_git.sh -list             List all tags available in the repository"
            echo ""
            echo -e "${BOLD}Options:${NC}"
            echo "  -tag, --tag <tag>     Tag to checkout (e.g. v1.0.1, office-sync-1)"
            echo "  -l, --list            Show all tags and exit"
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

echo -e "${CYAN}======================================================${NC}"
echo -e "${BOLD}LPC_Revive: Retrieving from Git${NC}"
echo -e "  Repository: ${BOLD}${ROOT_DIR}${NC}"
echo -e "  Remote:     ${BOLD}https://github.com/satishvspatika/LPC_Revive${NC}"
echo -e "${CYAN}======================================================${NC}"

# 1. Fetch latest commits and tags from remote
echo -e "\n${YELLOW}1. Fetching latest updates and tags from remote...${NC}"
git fetch origin --tags --prune
echo -e "${GREEN}   Fetch completed.${NC}"

# Handle tag listing
if [ "$LIST_TAGS" -eq 1 ]; then
    echo -e "\n${BOLD}Available Tags in Repository:${NC}"
    if git tag -l | grep -q .; then
        git tag -n -l --sort=-creatordate
    else
        echo "  (No tags found in repository yet)"
    fi
    exit 0
fi

# Check for uncommitted local changes before switching or pulling
UNCOMMITTED="$(git status --porcelain)"
if [ -n "$UNCOMMITTED" ]; then
    echo -e "\n${YELLOW}[WARNING] You have uncommitted local modifications:${NC}"
    git status -s
    echo ""
    echo -n "Do you want to stash your local changes before proceeding? [y/N]: "
    read -r STASH_REPLY
    if [[ "$STASH_REPLY" =~ ^[Yy]$ ]]; then
        STASH_MSG="Auto-stash by get_git.sh on $(date '+%Y-%m-%d %H:%M:%S')"
        git stash push -m "$STASH_MSG"
        echo -e "${GREEN}   Changes stashed successfully (use 'git stash pop' to recover).${NC}"
    else
        echo -e "${RED}[ERROR] Please commit, stash, or discard your local changes first. Aborted.${NC}"
        exit 1
    fi
fi

# 2. If a specific tag was requested, checkout that tag
if [ -n "$TAG_NAME" ]; then
    echo -e "\n${YELLOW}2. Checking out tag '${TAG_NAME}'...${NC}"
    if ! git rev-parse "refs/tags/${TAG_NAME}" >/dev/null 2>&1; then
        echo -e "${RED}[ERROR] Tag '${TAG_NAME}' was not found in this repository!${NC}"
        echo -e "${BOLD}Available tags:${NC}"
        git tag -l | sed 's/^/  - /'
        exit 1
    fi

    # Checkout the tag
    git checkout "tags/${TAG_NAME}"

    echo -e "\n${GREEN}======================================================${NC}"
    echo -e "${BOLD}${GREEN}✔ Successfully checked out tag '${TAG_NAME}'!${NC}"
    echo -e "  Commit: $(git log -1 --oneline)"
    echo -e "${GREEN}======================================================${NC}"
    echo -e "${CYAN}Note:${NC} You are currently at a specific tag snapshot."
    echo -e "If you want to start making new changes from here, you can create a branch:"
    echo -e "  ${BOLD}git checkout -b branch-${TAG_NAME}${NC}"
    echo -e "Or return to the latest main branch anytime with:"
    echo -e "  ${BOLD}git checkout main && ./get_git.sh${NC}\n"

else
    # Default mode: Retrieve latest commit on main/current branch
    CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")"

    if [ "$CURRENT_BRANCH" = "HEAD" ]; then
        echo -e "\n${YELLOW}2. Switching back from detached tag to 'main' branch...${NC}"
        git checkout main
        CURRENT_BRANCH="main"
    fi

    echo -e "\n${YELLOW}2. Pulling latest commits on branch '${CURRENT_BRANCH}'...${NC}"
    git pull origin "$CURRENT_BRANCH"

    echo -e "\n${GREEN}======================================================${NC}"
    echo -e "${BOLD}${GREEN}✔ Successfully updated to latest on branch '${CURRENT_BRANCH}'!${NC}"
    echo -e "  Latest Commit: $(git log -1 --oneline)"
    echo -e "${GREEN}======================================================${NC}"

    # Show recent tags if any
    LATEST_TAGS="$(git tag -l --sort=-creatordate | head -n 3)"
    if [ -n "$LATEST_TAGS" ]; then
        echo -e "Recent tags available:"
        echo "$LATEST_TAGS" | sed 's/^/  - /'
        echo -e "Use ${BOLD}./get_git.sh -tag \"<tag>\"${NC} to checkout any specific tag.\n"
    fi
fi

