SELECT T.name
FROM CatchedPokemon CP JOIN Trainer T
ON CP.owner_id = T.id
WHERE EXISTS (
  SELECT *
  FROM CatchedPokemon CP2
  WHERE CP.pid = CP2.pid AND CP.nickname <> CP2.nickname AND CP2.owner_id = T.id)
GROUP BY T.name 
HAVING COUNT(*) >= 2
ORDER BY T.name ASC