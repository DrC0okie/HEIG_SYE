# Rapport laboratoire 

## A) Implementing do_renice

```C
int do_renice(uint32_t pid, uint32_t new_prio) {
	//TO COMPLETE
	if(pid < 0 || new_prio < 1) {
		return -1;
	} else {
		pcb_t* pcb = find_proc_by_pid(pid);
		pcb->main_thread->prio = new_prio;
	}

	return 0;
}
```



## B) time_loop en arrière-plan

Quand la commande `time_loop` est executée dans le shell, rien ne se passe jusqu'à ce qu'une autre commande est executée:

```
so3% time_loop &
[2]
so3% ls
[2] 0
(...)
[2] 29
dev/
reds.bin
cat.elf
echo.elf
hello.elf
ls.elf
sh.elf
sleep.elf
test_fork2.elf
time_loop.elf
toupper.elf
[2] Terminated
so3% 
```

Le processus `time_loop`, une fois lancé en arrière-plan, ne démarre pas immédiatement. Il reste en état `Ready` sans passer à `Running`. Ceci est dû au fait que le processus shell, qui le lance, ne réalise pas de `waitpid`, le laissant ainsi en arrière-plan. Le shell, en conséquence, continue de fonctionner en état `Running`. De plus, étant donné que `time_loop` et le shell ont la même priorité, l'ordonnanceur ne fait pas de préemption.

Lorsqu'une nouvelle commande est exécutée pendant que `time_loop` est en arrière-plan, le shell, toujours actif, traite cette commande. Comme `time_loop` et le shell partagent la même priorité et que le shell n'attend pas activement `time_loop`, l'ordonnanceur continue de privilégier le shell. Ainsi, `time_loop` n'accède pas immédiatement au processeur et reste inactif jusqu'à ce que l'ordonnanceur lui attribue des ressources, probablement après que la commande en cours dans le shell soit terminée ou interrompue.

## C) Changement de priorité du shell

Pour changer la priorité du shell, nous pouvons utiliser la fonction `renice` que nous venons d'implémenter. Le shell a un PID de 1 et une priorité de 10. Nous allons donc définir une priorité de 9  pour le PID 1 avec la commande suivante : `renice 1 9`. Ensuite, nous pouvons exécuter `time_loop &` comme auparavant et remarquer que cette fois-ci, l'exécution est instantanée.

Dès que `time_loop` est exécuté, et parce qu'il a une priorité plus élevée que le shell, le shell passe en état `Waiting` et `time_loop` passe en état `Running` jusqu'à ce qu'il ait terminé son  opération et passe à l'état de "zombie". Ensuite, le shell repasse en  état `Running`. Ceci est dû au fait que lorsqu'il y a  plusieurs processus avec des priorités différentes, un réordonnancement  est effectué.



## D) time_loop in blocking mode

Si on change la priorité du shell et que l'on lance `time_loop` en mode "bloquant", le compteur prendra le dessus sur le shell et on ne peut plus  revenir sur le shell complètement. On peut écrire sur le shell, mais le  compteur continue. On retrouve alors des passages entre le shell et le  compteur lorsque l'on esaie d'écrire.

En mode bloquant, P(`time_loop`) utilise une attente passive (avec un `sleep`). Plutôt que de passer en état `Waiting`, le processus reste en état `Ready`. Cependant, le syscall `usleep` déclenche un changement de contexte, permettant ainsi à l'ordonnanceur de réévaluer l'état du processus.

 Lorsque `time_loop` a une priorité inférieure ou égale à celle du shell, le shell conserve son statut de processus actif. En revanche, si `time_loop` possède une priorité supérieure à celle du shell, il est alors priorisé lors du changement de contexte.
