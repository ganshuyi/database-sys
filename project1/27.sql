SELECT T.name, MAX(CP.level)
FROM CatchedPokemon CP, Trainer T
WHERE CP.owner_id = T.id
GROUP BY T.name
HAVING COUNT(*) >= 4
ORDER BY T.name ASC