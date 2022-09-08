# **Project 1: SQL Practice**

### **Task:**
Write the appropriate **SQL queries** for the following problems using the provided Pokemon Database Schema (File: insert.txt).

### **Problems**

Q1) 잡은포켓몬이3마리이상인트레이너의이름을잡은포켓몬의수가많은순서대로출력하세요.
Print the name of trainers tht caught 3 or more pokemons by the order of number of caught pokemons in descending order

Q2) 전체포켓몬중가장많은2개의타입을가진포켓몬이름을사전순으로출력하세요.
Print names of pokemon types where the types are the most and 2nd most in number in alphabetical order
 
Q3) 고향이상록시티인트레이너들이가지고있는‘Electric’ type의포켓몬레벨의평균을출력하세요.
Print the average level of Electric type pokemons owned by trainers from Sangnok city

Q4) 잡은포켓몬중레벨이50 이상인포켓몬의닉네임을사전순으로출력하세요.
Print the nicknames of pokemons which are caught that are of level 50 and above in alphabetical order

Q5) 체육관관장을맡고있지않은트레이너의이름을사전순으로출력하세요.
Print the names of the trainers that do not hold the position of gym manager in alphabetical order

Q6) Blue City 출신의트레이너를사전순으로출력하세요.
Print the trainers that are born in Blue City in alphabetical order

Q7) 트레이너의이름을트레이너의고향의사전순으로출력하세요.
Print the names of the trainers in alphabetical order of their hometown

Q8) Red가잡은포켓몬의평균레벨을출력하세요.
Print the average level of pokemons caught by Red

Q9) 체육관관장이름과, 관장이가진포켓몬레벨의평균을 관장이름의오름차순순서대로 출력하세요.
Print the name of gym manager and the average level of pokemons owned by the manager by the ascending order of gym manager name

Q10) 잡은포켓몬중레벨이50 이상이고owner_id가6 이상인포켓몬의닉네임을 사전순으로출력하세요.
Print the nickname of caught pokemons that are level 50 and above, and owner_id is 6 and above in alphabetical order

Q11) 포켓몬도감에있는모든포켓몬의ID와이름을ID에오름차순으로정렬해출력하세요.
Print all IDs and names of pokemons tht are in the pokemon illustrated guide in ascending order of their ID

Q12) 잡힌포켓몬중레벨이30 이상인포켓몬의이름과타입을포켓몬이름의사전순으로중복없이
출력하세요.
Print the names and types of caught pokemons that are of level 30 and above in alphabetical order of their names without repetition

Q13) 상록시티출신트레이너가가진포켓몬들의이름과포켓몬ID를포켓몬ID의오름차순으로정렬해출력하세요.
Print the names and IDs of pokemons that are caught by trainers from Sangnok city in ascending order of their IDs

Q14) 풀타입포켓몬중진화가능한포켓몬의이름을출력하시오
Print the names of grass-type pokemons that are able to evolve
 
Q15)포켓몬을가장많이잡은트레이너의id와그트레이너가잡은포켓몬의수를출력하세요. 
Print the ID of the pokemon trainer that caught the most pokemons and the number of pokemons that he caught (if there are more than 1 trainer that caught the most number of pokemons, sort them in ascending order of their IDs)

Q16) 포켓몬도감에있는포켓몬중’Water’타입, ‘Electric’타입, ‘Psychic’타입포켓몬의총합을출력하세요.
Print the total of pokemons of water type, electric type and psychic type from the illustrated guide of pokemons

Q17) 상록시티출신트레이너들이가지고있는포켓몬종류의개수를출력하세요.
Print the number of pokemon types that are owned by trainers from Sangnok city

Q18) 각체육관의관장에게잡힌전체포켓몬의평균레벨을출력하세요.
Print the average level of pokemons that are caught by each gym manager
 
Q19) 상록시티의리더가가지고있는포켓몬타입의개수를출력하세요.
Print the number of pokemon types that the leader of Sangnok city owns
 
Q20)상록시티출신트레이너의이름과각트레이너가잡은포켓몬수를잡은포켓몬수에대해오름차순 순서대로출력하세요.
Print the names of trainers from Sangnok city and the number of the pocketmons they caught in ascending order (of number of pokemons)

Q21) 관장들의이름과각관장이잡은포켓몬의수를관장이름의오름차순으로출력하세요.
Print the names of managers and the number of pokemons they caught in ascending order (of the name of the manager)

Q22) 포켓몬의타입과해당타입을가지는포켓몬의수가몇마리인지를출력하세요. 수에대해 오름차순으로출력하세요. (같은수를가진경우타입명의사전식오름차순으로정렬)
Print the pokemon types and the number of pokemons of each type in ascending order of the number (for the same amount sort by the alphabetical order of their names)
  
Q23) 잡힌포켓몬중레벨이10이하인포켓몬을잡은트레이너의이름을중복없이사전순으로출력하세요.
Print the names of trainers that caught pokemons of level 10 and below (without repetition) in alphabetical order of their names

Q24) 각시티의이름과해당시티의고향출신트레이너들이잡은포켓몬들의평균레벨을평균레벨에
오름차순으로정렬해서출력하세요.
Print the name of all cities and the average level of pocketmons caught by trainers from each city in ascending order of the levels

Q25)상록시티출신트레이너와브라운시티출신트레이너가공통으로잡은포켓몬의이름을사전순으로 출력하세요. (중복은제거할것)
Print the names of pokemons that are caught by both Sangnok city and Brown city trainers in alphabetical order (remove repetitions)
 
Q26) 잡힌포켓몬중닉네임에공백이들어가는포켓몬의이름을사전식내림차순으로출력하세요.
Print the names of caught pokemons tht have spaces in their nicknames in reverse alphabetical order

Q27)포켓몬을4마리이상잡은트레이너의이름과해당트레이너가잡은포켓몬중가장레벨이높은 포켓몬의레벨을트레이너의이름에사전순으로정렬해서출력하세요.
Print the names of trainers that caught 4 pokemons and above and the highest level of pokemon they caught in alphabetical order (of their names)

Q28) ‘Normal’ 타입혹은‘Electric’ 타입의포켓몬을잡은트레이너의이름과 해당포켓몬의평균레벨을 구한평균레벨에오름차순으로정렬해서출력하세요.
Print the name of trainers that caught pokemons of type Normal or Electric and the average levels of the pokemons in ascending order (of average level)

Q29) 각타입별로잡힌포켓몬의수를타입이름의오름차순으로출력하세요.
Print the number of caught pokemons of each type in alphabetical order of the type names

Q30) 포켓몬중3단진화가가능한포켓몬의ID 와해당포켓몬의이름을1단진화형태포켓몬의이름, 2단 진화형태포켓몬의이름, 3단진화형태포켓몬의이름을 ID의오름차순으로출력하세요.
Print the IDs of pokemons that are able to evolve 3 levels and their names in 1st , 2nd and 3rd  level evolution in ascending order (of IDs)

Q31) 진화가능한전체포켓몬을타입으로분류한후포켓몬의수가3개이상인타입을사전역순으로 출력하세요.
Sort the pokemons that are capable of evolution by their types. Then print the types that have 3 or more pokemons in reverse alphabetical order (of their types) 

Q32) 어느트레이너에게도잡히지않은포켓몬의이름을사전순으로출력하세요.
Print the names of pokemons that are not caught by any trainer in alphabetical order

Q33) 트레이너Matis가잡은포켓몬들의레벨의총합을출력하세요.
Print the total of pokemon levels that are caught by trainer Matis 

Q34) 체육관관장이잡은포켓몬중, 닉네임이A로시작하는포켓몬의이름과레벨, nickname을 내림차순순서대로출력하세요.
Print the name, level and nicknames of pokemons caught by gym managers that have nicknames that start with the letter A in descending order (of nicknames)

Q35) 진화후의id가더작아지는포켓몬의진화전이름을사전순으로출력하세요.
Print the names (pre-evolution) of pokemons which IDs have decreased in size after evolution in alphabetical order 

Q36) 진화가가능한포켓몬중최종진화형태의포켓몬을잡은트레이너의이름을오름차순으로 출력하세요.
Print the names of trainers that caught pokemons that have evolved to the final stage in alphabetical order

Q37) 잡은포켓몬레벨의총합이가장높은트레이너의이름과그총합을출력하세요.
Print the name of the trainer who has the highest total of caught pokemon level and the total level

Q38) 진화가능한포켓몬중최종진화형태의포켓몬의이름을오름차순으로출력하세요.
Print the names of pokemons that have evolved to the final stage in ascending order

Q39) 동일한포켓몬을두마리이상잡은트레이너의이름을사전순으로출력하세요.
Print the names of trainers that caught two or more identical pokemons in alphabetical order 

Q40) 출신도시명과, 각출신도시별로트레이너가잡은가장레벨이높은포켓몬의별명을 출신도시명의 사전순으로출력하세요.
Print the hometown names and the nickname of the pokemon with the highest level caught by the trainer of each city in alphabetical order (of hometown name)
