# Contributing

Nothing reaches `main` without a pull request, green CI, and one approving review.

1. **Branch** from an up-to-date `main`: `git switch main && git pull && git switch -c yourname/short-topic`
2. **Commit** small changes; before pushing, run `tools/format.sh` and `cmake --build --preset dev && ctest --preset dev`
3. **Open a PR**: `git push -u origin HEAD`, open the link it prints, fill in the template, link the issue ("Closes #12")
4. **Review**: one approval from someone outside your pair, and all three CI checks green; push fixes to the same branch
5. **Merge** on GitHub; the branch is deleted for you. Then `git switch main && git pull`
