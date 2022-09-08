SELECT P.name
FROM Evolution E, Pokemon P
WHERE P.id = E.after_id
  AND EXISTS (
  SELECT *
  FROM Evolution E2
  WHERE E.before_id = E2.after_id) 
ORDER BY P.name ASC