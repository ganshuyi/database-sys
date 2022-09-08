SELECT P.type, COUNT(*) AS cnt
FROM Pokemon P
GROUP BY P.type
HAVING COUNT(*) = (SELECT MAX(no1)
                   FROM (
                   SELECT COUNT(*) no1
                   FROM Pokemon P
                   GROUP BY P.type) as table1)
UNION ALL
SELECT P.type, COUNT(*) AS cnt
FROM Pokemon P
GROUP BY P.type
HAVING cnt = (SELECT COUNT(*) as count
              FROM Pokemon P
              GROUP BY type
              ORDER BY count desc limit 1,1)