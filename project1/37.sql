SELECT T.name, SUM(CP.level)
FROM CatchedPokemon CP, Trainer T
WHERE CP.owner_id = T.id 
GROUP BY T.name
ORDER BY SUM(CP.level) DESC LIMIT 1