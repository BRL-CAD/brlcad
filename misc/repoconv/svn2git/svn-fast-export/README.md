svn-all-fast-export aka svn2git
===============================
This project contains all the tools required to do a conversion of an svn repository (server side, not a checkout) to one or more git repositories.

This is the tool used to convert KDE's Subversion into multiple Git repositories.  You can find more description and usage examples at https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git


How does it work
----------------
The svn2git repository gets you an application that will do the actual conversion.
The conversion exists of looping over each and every commit in the subversion repository and matching the changes to a ruleset after which the changes are applied to a certain path in a git repo.
The ruleset can specify which git repository to use and thus you can have more than one git repository as a result of running the conversion.
Also noteworthy is that you can have a rule that, for example, changes in svnrepo/branches/foo/2.1/ will appear as a git-branch in a repository.

If you have a proper ruleset the tool will create the git repositories for you and show progress while converting commit by commit.

After it is done you likely want to run `git repack -a -d -f` to compress the pack file as it can get quite big.

Running as Docker image
-----------------------
Just mount your SVN folder, plus another working directory where Git repository will be created.
Sample usage with input mounted in /tmp and output produced in /workdir:
```
docker build -t svn2git .
docker run --rm -it -v `pwd`/workdir:/workdir -v /var/lib/svn/project1:/tmp/svn -v `pwd`/conf:/tmp/conf svn2git /usr/local/svn2git/svn-all-fast-export --identity-map /tmp/conf/project1.authors --rules /tmp/conf/project1.rules --add-metadata --svn-branches --debug-rules --svn-ignore --empty-dirs /tmp/svn/ 
```

Building the tool
-----------------
Run `qmake && make`.  You get `./svn-all-fast-export`.
(Do a checkout of the repo .git' and run qmake and make. You can only build it after having installed libsvn-dev, and naturally Qt. Running the command will give you all the options you can pass to the tool.)

You will need to have some packages to compile it. For Ubuntu distros, use this command to install them all:
`sudo apt-get install build-essential subversion git qtchooser qt5-default libapr1 libapr1-dev libsvn-dev`

To run all tests you can simply call the `test.sh` script in the root directory.
This will run all [Bats](https://github.com/bats-core/bats-core) based tests
found in `.bats` files in the directory `test`. Running the script will automatically
execute `qmake` and `make` first to build the current code if necessary.
If you want to run tests without running make, you can give `--no-make` as first parameter.
If you want to only run a subset of the tests, you can specify the base-name of one
or multiple `.bats` files to only run these tests like `./test.sh command-line svn-ignore`.
If you want to investigate the temporary files generated during a test run,
you can set the environment variables `BATSLIB_TEMP_PRESERVE=1` or `BATSLIB_TEMP_PRESERVE_ON_FAILURE=1`.
So if for example some test in `svn-ignore.bats` failed, you can investigate the failed case like
`BATSLIB_TEMP_PRESERVE_ON_FAILURE=1 ./test.sh --no-make svn-ignore` and then look
in `build/tmp` to investigate the situation.

KDE
---
there is a repository kde-ruleset which has several example files and one file that should become the final ruleset for the whole of KDE called 'kde-rules-main'.

Write the Rules
---------------
You need to write a rules file that describes how to slice the Subversion history into Git repositories and branches. See https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git.
The rules are also documented in the 'samples' directory of the svn2git repository. Feel free to add more documentation here as well.

Rules
-----
### `create respository`

```
create repository REPOSITORY NAME
  [PARAMETERS...]
end repository
```

`PARAMETERS` is any number of:

- `repository TARGET REPOSITORY` Creates a forwarding repository , which allows for redirecting to another repository, typically with some `prefix`.
- `prefix PREFIX` prefixes each file with `PREFIX`, allowing for merging repositories.
- `description DESCRIPTION TEXT` writes a `DESCRIPTION TEXT` to the `description` file in the repository

### `match`

```
match REGEX
  [PARAMETERS...]
end match
```

Creates a rule that matches paths by `REGEX` and applies some `PARAMETERS` to them. Matching groups can be created, and the values used in the parameters.

`PARAMETERS` is any number of:

- `repository TARGET REPOSITORY` determines the repository
- `branch BRANCH NAME` determines which branch this path will be placed in. Can also be used to make lightweight tags with `refs/tags/TAG NAME` although note that tags in SVN are not always a single commit, and will not be created correctly unless they are a single copy from somewhere else, with no further changes. See also `annotate true` to make them annotated tags.
- `[min|max] revision REVISION NUMBER` only match if revision is above/below the specified revision number
- `prefix PREFIX` prefixes each file with `PREFIX`, allowing for merging repositories. Same as when used in a `create repository` stanza.
  - Note that this will create a separate commit for each prefix matched, even if they were in the same SVN revision.
- `substitute [repository|branch] s/PATTERN/REPLACEMENT/` performs a regex substitution on the repository or branch name. Useful when eliminating characters not supported in git branch names.
- `action ACTION` determines the action to take, from the below three:

  - `export` I have no idea what this does
  - `ignore` ignores this path
  - `recurse` tells svn2git to ignore this path and continue searching its children.

- `annotated true` creates annotated tags instead of lightweight tags. You can see the commit log with `git tag -n`.

### `include FILENAME`

Include the contents of another rules file

### `declare VAR=VALUE`

Define variables that can be referenced later. `${VAR}` in any line will be replaced by `VALUE`.


Work flow
---------
Please feel free to fill this section in.

Some SVN tricks
---------------
You can access your newly rsynced SVN repo with commands like `svn ls file:///path/to/repo/trunk/KDE`.
A common issue is tracking when an item left playground for kdereview and then went from kdereview to its final destination. There is no straightforward way to do this. So the following command comes in handy: `svn log -v file:///path/to/repo/kde-svn/kde/trunk/kdereview | grep /trunk/kdereview/mplayerthumbs -A 5 -B 5` This will print all commits relevant to the package you are trying to track. You can also pipe the above command to head or tail to see the first and last commit it was in that directory.
