SELECT AVG(CP.level)
FROM CatchedPokemon CP
WHERE CP.owner_id IN (
  SELECT T.id
  FROM Trainer T
  WHERE T.name = "Red")