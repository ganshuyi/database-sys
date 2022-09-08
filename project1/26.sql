SELECT P.name
FROM Pokemon P, CatchedPokemon CP
WHERE P.id = CP.pid AND CP.nickname LIKE "% %"
ORDER BY P.name DESC