# git-crypt decrypt tool

__Why I write this script?__

I encrypt the git-crypt files when it have encrypted already (the old key)......

I do something like this:

```
git subtree add bababa...
.....
```

Then I can not decrypt this file:

```
encrypted(repoA_key, encrypted(repoB_key, myfile))
```

So I write this script `git-crypt-unlock.py`

```
$ pip install pycrypto

$ git-crypt-unlock.py --key .git/git-crypt/keys/default myfile
Info: AES  = ...
Info: HMAC = ...

Info: dump myfile.gcrypt-plain
Info: 100.00% 1/1 OK

$ git-crypt-unlock.py --key ../repoB/.git/git-crypt/keys/default myfile.gcrypt-plain
Info: AES  = ...
Info: HMAC = ...

Info: dump myfile.gcrypt-plain.gcrypt-plain
Info: 100.00% 1/1 OK
```


