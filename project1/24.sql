SELECT C.name, AVG(CP.level)
FROM City C, CatchedPokemon CP, Trainer T
WHERE C.name = T.hometown AND CP.owner_id = T.id
GROUP BY C.name
ORDER BY AVG(CP.level) ASC