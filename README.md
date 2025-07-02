# EC-SBT: a retargetable static binary translator

EC-SBT is a retargetable static binary translator based on [McSema](https://github.com/lifting-bits/mcsema).

EC-SBT can translate an elf from x86_64 to aarch64.


# Dependencies
to use this tool,you have to install :

| Name                                                       | Version |
| ---------------------------------------------------------- | ------- |
| [EFACT](https://github.com/solecnugit/EFACT)                                | Latest  |
| [CDMF](https://github.com/wuwen03/CMDF)                                | Latest   |
| [McSema](https://github.com/lifting-bits/mcsema)                            | Latest   |
| [llvm](https://github.com/llvm/llvm-project)                            | 20   |

1.when installing  [McSema](https://github.com/lifting-bits/mcsema),please first follow the standard guide to download related codes, before make the project ,please replace the soucre code in this project with the related dir (Mcsema was archived by the owner, directly repalce may save you a lot of time).

2.after install the above three project, please compile the passes with the guide under /Passes.
 
3.follow the guide in script/EC-SBT-Runner-base to  specify the dir of the current paltfrom.

4.run the EC-SBT:
```bash
$ ./EC-SBT-Runner-base targetELF
```


