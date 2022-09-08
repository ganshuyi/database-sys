SELECT T.name, COUNT(CP.id)
FROM Trainer T, CatchedPokemon CP
WHERE T.id = CP.owner_id
GROUP BY T.id
HAVING COUNT(CP.id) >= 3