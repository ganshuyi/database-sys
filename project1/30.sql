SELECT P.id, P.name AS "1st name"
FROM Evolution E, Pokemon P
WHERE P.id = E.before_id AND EXISTS (
  SELECT *
  FROM Evolution E2
  WHERE E.after_id = E2.before_id)
ORDER BY P.id ASC