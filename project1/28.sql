SELECT T.name, AVG(CP.level)
FROM CatchedPokemon CP, Pokemon P, Trainer T
WHERE CP.owner_id = T.id AND CP.pid = P.id AND (P.type = "Normal" OR P.type = "Electric")
GROUP BY T.name
ORDER BY AVG(CP.level) ASC