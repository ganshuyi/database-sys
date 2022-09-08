SELECT DISTINCT P.name, P.type
FROM CatchedPokemon CP, Pokemon P
WHERE CP.pid = P.id AND CP.level >= 30
ORDER BY P.name ASC