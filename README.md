# LGSI-TEE-Daily-Progress
Progress Tracker for LGSI TEE project 
## Team Members
- Member 1 - Avanthik C
- Member 2 - Amith K S
- Member 3 - Sathya Prathmesh K J
- Mentor - Jevitha K.P

## Setting Up the repo 
Clone the following into your desired folder 

```git clone https://github.com/avanthik-c/LGSI-TEE-Daily-Progress.git```

## Steps for modifying (Read Carefully!)
To keep the repository organized and avoid "Merge Conflicts," please follow these steps every time you work.

### 1. Start of Session: Sync Your Local Copy

Before you start typing or coding, always get the latest updates from your teammates:
```bash
git pull origin main
```

### 2. Making Changes
Only edit files within your assigned folder. 
*If you create a new folder (e.g., for a new sub-topic), make sure to add at least one file (like a README.md or .gitkeep) inside it. Git will not track empty folders.

### 3. End of Session: Save and Push
When you are ready to upload your work:

```Bash
# 1. Stage your changes
git add [Your-Name-Folder]/

# 2. Commit with a meaningful message
git commit -m "Update progress: [Briefly describe what you added]"

# 3. Push to GitHub
git push origin main
```

## Troubleshooting: Handling Merge Conflicts

If you see an error saying REJECTED when you push, it means a teammate pushed changes while you were working.
Run 
`git pull origin main.`
Open the files that Git says have "Conflicts."
Look for the markers:

```Plaintext
<<<<<< HEAD
(Your local version)
=======
(The version already on GitHub)
>>>>>> main
```
Delete the markers, keep the correct text, save the file, and then add, commit, and push again.

## Guidelines for Members
Format: Use Markdown (.md) for all notes to ensure they are readable on GitHub.

Images: Keep screenshots in a screenshots/ subfolder within your own directory. 
Link them using: 

`![](screenshots/image_name.png).`


## Mentor Feedback
The resources/remarks/ folder is reserved for the mentor. 
Members should check this folder regularly for feedback on their research and technical implementations.
