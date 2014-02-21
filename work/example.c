int logorg_parity_disk (logorg *currlogorg, ioreq_event *curr, int numreqs)
{
   depends *depend;

   if (curr->flags & READ) {
      return(numreqs);
   }
   if ((currlogorg->maptype == STRIPED) && (currlogorg->stripeunit == 0)) {
      curr->next = ioreq_copy(curr);
      curr->next->devno = currlogorg->numdisks;
      return(numreqs+1);
   }
   if (numreqs != 1) {
      fprintf(stderr, "Too many reqs at logorg_parity_disk - %d\n", numreqs);
      exit(1);
   }
   depend = (depends *) getfromextraq();
   depend->blkno = curr->blkno;
   depend->devno = curr->devno;
   depend->numdeps = 2;
   depend->deps[0] = ioreq_copy(curr);
   depend->deps[1] = ioreq_copy(curr);
   depend->deps[0]->opid = 2;
   depend->deps[0]->devno = currlogorg->numdisks;
   depend->next = (depends *) getfromextraq();
   depend->next->next = NULL;
   depend->next->blkno = curr->blkno;
   depend->next->devno = currlogorg->numdisks;
   depend->next->deps[0] = depend->deps[0];
   depend->next->deps[1] = depend->deps[1];
   if (currlogorg->writesync == TRUE) {
      depend->deps[1]->opid = 2;
      depend->next->numdeps = 2;
   } else {
      depend->deps[1]->opid = 1;
      depend->next->numdeps = 1;
   }
   curr->flags |= READ;
   curr->next = ioreq_copy(curr);
   curr->next->devno = currlogorg->numdisks;
   curr->prev = (ioreq_event *) depend;
   return(4);
}
