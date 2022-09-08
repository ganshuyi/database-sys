SELECT T.id, COUNT(*)
FROM Trainer T, CatchedPokemon CP
WHERE T.id = CP.owner_id
GROUP BY T.id
HAVING COUNT(*) IN (
  SELECT MAX(mycount)
  FROM (
    SELECT COUNT(*) mycount
    FROM CatchedPokemon CP, Trainer T
    WHERE CP.owner_id = T.id 
    GROUP BY T.id) AS my
  )
ORDER BY T.id ASC 