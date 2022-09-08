SELECT T.name
FROM Evolution E, Pokemon P, CatchedPokemon CP, Trainer T
WHERE P.id = E.after_id AND CP.pid = P.id AND T.id = CP.owner_id
  AND EXISTS (
  SELECT *
  FROM Evolution E2
  WHERE E.before_id = E2.after_id) 
ORDER BY T.name ASC
  