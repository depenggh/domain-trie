# 1 mil domain name for iprtree
with optimization: only build the iprtree once at the end.
```
Iprtree:

inserting 1000000 patterns: time: 1 sec, memory: 420992 KB

building tree for 1000000 patterns: time: 4 sec, memory: 4519552 KB

searching 1000000 patterns: time: 2 sec

```

# 1 mil domain name for patricia trie
```
Patricia Trie:

inserting 1000000 patterns: time: 9 sec, memory: 1563392 KB

sarching 1000000 patterns: time: 6 sec
```


# How this patricia trie implemtnation works


Given
```
1. *.sc.ciscoplus.com : 123
2. *.use.ciscoplus.com : 456
```

first, insert into trie hash table:

|suffix              |    Key   |  Value (index into labels vector) |
|:------------------:|:--------:|:------:|
| com                |    1     |   0    |
| ciscoplus.com      |    2     |   1    |
| sc.ciscoplus.com   |    3     |   2    |
| *.sc.ciscoplus.com |    4     |   3    |


|suffix              |    Key   |  Value (index into labels vector) |
|:------------------:|:--------:|:------:|
|com                 |    1     |   0    |
|ciscoplus.com       |    2     |   1    |
|use.ciscoplus.com   |    5     |   4    |
|*.use.ciscoplus.com |    6     |   3    |


second, save lables to vector, so to be shared by all

|label     |  index |
|:--------:|:------:|
|com       |   0    |
|ciscoplus |   1    |
|sc        |   2    |
|*         |   3    |
|use       |   4    |


save label's index into label hash table:

|key         |  index into labels vector |
|:----------:|:-------------------------:|
|com         |      0                    |
|ciscoplus   |      1                    |
|sc          |      2                    |
|*           |      3                    |
|use         |      4                    |


third, insert backendsets to backend hasht able:

|key    |  backendsets |
|:-----:|:------------:|
|4      |   123        |
|6      |   456        |


# Questions
1. hash table collision
2. optomal value for load factor
